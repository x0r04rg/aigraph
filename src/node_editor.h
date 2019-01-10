
#include <SDL2/SDL_rwops.h>
#include "aigraph.h"
#include "console.h"

#define NODE_WIDTH 180.0f

typedef enum { LINK_INBOUND, LINK_OUTBOUND } link_type;
typedef enum { MARK_NONE, MARK_TEMPORARY, MARK_PERMAMENT } node_mark;

struct node_link {
    link_type type;
    int slot;
    int other_id;
    int other_slot;
};

struct node_link_list {
    size_t size, capacity;
    struct node_link *links;
};

union node_property
{
    int i;
    float f;
    short e;
};

struct node {
    node_type type;
    struct nk_rect bounds;
    struct node_link_list links;
    float *consts;
    union node_property *props;
    node_mark mark; // needed for topological search
};

struct node_linking {
    int active;
    int input_id;
    int input_slot;
};

struct node_editor {
    struct config *conf;
    struct console *console;
    struct node *nodes;
    size_t node_count, nodes_capacity;
    struct nk_rect bounds;
    int selected_id;
    struct nk_vec2 scrolling;
    struct node_linking linking;
};

static float
line_dist(float ax, float ay, float bx, float by, float cx, float cy)
{
    float abx = bx - ax;
    float aby = by - ay;
    float acx = cx - ax;
    float acy = cy - ay;
    float bcx = cx - bx;
    float bcy = cy - by;
    if (abx * acx + aby * acy < 0)
    {
        return sqrtf(acx * acx + acy * acy);
    }
    else if (-abx * bcx + -aby * bcy < 0)
    {
        return sqrtf(bcx * bcx + bcy * bcy);
    }
    else
    {
        float px = -aby;
        float py = abx;
        float proj = fabsf(acx * px + acy * py);
        return proj ? (proj / sqrtf(acx * acx + acy * acy)) : 0;
    }
}

static int
is_hovering_curve(float mx, float my, float ax, float ay, float c1x, float c1y, 
    float c2x, float c2y, float bx, float by, int segments, float thickness)
{
    float x = ax, y = ay;
    float inc = 1.0f / segments;
    float t = inc;
    for (int i = 0; i < segments; i++)
    {
        float x1 = (1 - t) * (1 - t) * (1 - t) * ax + 3 * (1 - t) * (1 - t) * t * c1x + 3 * (1 - t) * t * t * c2x + t * t * t * bx;
        float y1 = (1 - t) * (1 - t) * (1 - t) * ay + 3 * (1 - t) * (1 - t) * t * c1y + 3 * (1 - t) * t * t * c2y + t * t * t * by;
        if (line_dist(x, y, x1, y1, mx, my) < thickness) return 1;
        x = x1;
        y = y1;
        t += inc;
    }
    return 0;
}

static inline void 
add_link(struct node_link_list *list, enum link_type type, int slot, int other_id, 
    int other_slot)
{
    if (list->size == list->capacity)
    {
        size_t new_capacity = list->capacity ? 2 * list->capacity : 5;
        list->links = realloc(list->links, new_capacity * sizeof(struct node_link));
        list->capacity = new_capacity;
    }
    struct node_link link;
    link.type = type;
    link.slot = slot;
    link.other_id = other_id;
    link.other_slot = other_slot;
    list->links[list->size++] = link;
}

static inline void
remove_link(struct node_link_list *list, int i)
{
    NK_ASSERT(i >= 0 && i < list->size);
    list->links[i] = list->links[--list->size];
}

static inline struct node_link*
get_link(struct node_link_list *list, int i)
{
    NK_ASSERT(i >= 0 && i < list->size);
    return &list->links[i];
}

static inline void
editor_print(struct node_editor *editor, char *string)
{
    console_print(editor->console, string);
}

static inline void
editor_printf(struct node_editor *editor, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_printfv(editor->console, fmt, args);
    va_end(args);
}

static void
node_editor_add(struct node_editor *editor, node_type type, float pos_x, float pos_y)
{
    struct node_info *info = &editor->conf->nodes[type];
    struct node *node;
    if (editor->node_count == editor->nodes_capacity)
    {
        size_t new_capacity = editor->nodes_capacity ? 2 * editor->nodes_capacity : 10;
        editor->nodes = realloc(editor->nodes, new_capacity * sizeof(struct node));
        editor->nodes_capacity = new_capacity;
    }
    node = &editor->nodes[editor->node_count++];
    memset(node, 0, sizeof *node);
    node->type = type;
    node->bounds = nk_rect(pos_x, pos_y, NODE_WIDTH, 30 * 
        (info->input_count + info->output_count + info->prop_count) + 35);
    node->consts = calloc(info->input_count, sizeof *node->consts);
    node->props = calloc(info->prop_count, sizeof *node->props);
}

static void 
node_editor_delete(struct node_editor *editor, int node_id)
{
    struct node *node = &editor->nodes[node_id];

    /* remove all links to node */
    for (int i = 0; i < node->links.size; ++i)
    {
        struct node_link *link = get_link(&node->links, i);
        struct node_link_list *links = &editor->nodes[link->other_id].links;
        for (int i = 0; i < links->size;)
        {
            if (get_link(links, i)->other_id == node_id) remove_link(links, i);
            else ++i;
        }
    }

    free(node->links.links);
    free(node->props);
    free(node->consts);

    *node = editor->nodes[--editor->node_count];

    if (node_id != editor->node_count)
    {
        /* patch all links to the node that was placed to node_id index */
        for (int i = 0; i < node->links.size; ++i)
        {
            struct node_link *link = get_link(&node->links, i);
            struct node_link_list *links = &editor->nodes[link->other_id].links;
            for (int i = 0; i < links->size; ++i)
            {
                struct node_link *link = get_link(links, i);
                if (link->other_id == editor->node_count) link->other_id = node_id;
            }
        }
    }
}

static void
node_editor_link(struct node_editor *editor, int in_id, int in_slot,
    int out_id, int out_slot)
{
    add_link(&editor->nodes[in_id].links, LINK_OUTBOUND, in_slot, out_id, out_slot);
    add_link(&editor->nodes[out_id].links, LINK_INBOUND, out_slot, in_id, in_slot);
}

static void
node_editor_unlink(struct node_editor *editor, int in_id, int in_slot,
    int out_id, int out_slot)
{
    struct node_link_list *links;

    links = &editor->nodes[in_id].links;
    for (int i = 0; i < links->size; ++i)
    {
        struct node_link *l = get_link(links, i);
        if (l->type == LINK_OUTBOUND && l->slot == in_slot &&
            l->other_id == out_id && l->other_slot == out_slot)
        {
            remove_link(links, i);
            break;
        }
    }

    links = &editor->nodes[out_id].links;
    for (int i = 0; i < links->size; ++i)
    {
        struct node_link *l = get_link(links, i);
        if (l->type == LINK_INBOUND && l->slot == out_slot &&
            l->other_id == in_id && l->other_slot == in_slot)
        {
            remove_link(links, i);
            break;
        }
    }
}

static void
node_editor_init(struct node_editor *editor, struct config *config, struct console *console)
{
    memset(editor, 0, sizeof(*editor));
    editor->selected_id = -1;
    editor->conf = config;
    editor->console = console;
}

static void
node_editor_cleanup(struct node_editor *editor)
{
    for (int i = 0; i < editor->node_count; ++i)
        if (editor->nodes[i].links.links)
            free(editor->nodes[i].links.links);
    free(editor->nodes);
}

static struct node_link*
find_node_input(struct node *node, int slot)
{
    for (int i = 0; i < node->links.size; ++i)
    {
        struct node_link *link = get_link(&node->links, i);
        if (link->type == LINK_INBOUND && link->slot == slot) return link;
    }
    return NULL;
}

static struct compiled_graph* node_editor_compile(struct node_editor *editor);
static int tsort(struct node *nodes, int node_count, struct node **sorted);

static int
node_editor_gui(struct nk_context *ctx, struct node_editor *nodedit, struct nk_rect win_size, 
    nk_flags flags)
{
    int n = 0;
    struct nk_rect total_space;
    const struct nk_input *in = &ctx->input;
    struct nk_command_buffer *canvas;
    int updated = -1;
    struct node_info *infos = nodedit->conf->nodes;

    if (nk_begin(ctx, "aigraph", win_size, flags))
    {
        /* allocate complete window space */
        canvas = nk_window_get_canvas(ctx);
        total_space = nk_window_get_content_region(ctx);
        nk_layout_space_begin(ctx, NK_STATIC, total_space.h, nodedit->node_count);
        {
            struct node *it;
            struct nk_rect size = nk_layout_space_bounds(ctx);
            struct nk_panel *node = 0;

            {
                /* display grid */
                float x, y;
                const float grid_size = 32.0f;
                const struct nk_color grid_color = nk_rgb(50, 50, 50);
                for (x = (float)fmod(size.x - nodedit->scrolling.x, grid_size); x < size.w; x += grid_size)
                    nk_stroke_line(canvas, x+size.x, size.y, x+size.x, size.y+size.h, 1.0f, grid_color);
                for (y = (float)fmod(size.y - nodedit->scrolling.y, grid_size); y < size.h; y += grid_size)
                    nk_stroke_line(canvas, size.x, y+size.y, size.x+size.w, y+size.y, 1.0f, grid_color);
            }

            /* execute each node as a movable group */
            for (int i = 0; i < nodedit->node_count; i++) {
                it = &nodedit->nodes[i];
                /* calculate scrolled node window position and size */
                nk_layout_space_push(ctx, nk_rect(it->bounds.x - nodedit->scrolling.x,
                    it->bounds.y - nodedit->scrolling.y, it->bounds.w, it->bounds.h));

                /* execute node window */
                if (nk_group_begin(ctx, infos[it->type].name, NK_WINDOW_MOVABLE|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE))
                {
                    /* always have last selected node on top */

                    node = nk_window_get_panel(ctx);
                    if (updated == -1 && i != nodedit->node_count - 1 && 
                        nk_input_mouse_clicked(in, NK_BUTTON_LEFT, node->bounds))
                    {
                        updated = i;
                    }

                    /* ================= NODE CONTENT =====================*/
                    nk_layout_row_dynamic(ctx, 25, 1);
                    struct node_info *info = &infos[it->type];
                    char pname[16];
                    for (int i = 0; i < info->output_count; ++i)
                    {
                        nk_label(ctx, info->outputs[i].name, NK_TEXT_ALIGN_RIGHT | NK_TEXT_ALIGN_MIDDLE);
                    }
                    for (int i = 0; i < info->prop_count; ++i)
                    {
                        sprintf_s(pname, NK_LEN(pname), "#%s", info->props[i].name);
                        switch (info->props[i].type)
                        {
                            case FIELD_INT: 
                                it->props[i].i = nk_propertyi(ctx, pname, -100, it->props[i].i, 100, 1, 1);
                                break;
                            case FIELD_FLOAT:
                                it->props[i].f = nk_propertyf(ctx, pname, -100, it->props[i].f, 100, 1, 1);
                                break;
                            case FIELD_ENUM:
                                {struct enum_info *e = &nodedit->conf->enums[info->props[i].enum_type];
                                it->props[i].e = nk_combo(ctx, e->values, e->count, it->props[i].e, 
                                    25, nk_vec2(nk_layout_widget_bounds(ctx).w, 200));}
                                break;
                        }
                    }
                    for (int i = 0; i < info->input_count; ++i)
                    {
                        if (find_node_input(it, i)) 
                        {
                            nk_label(ctx, info->inputs[i].name, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                        }
                        else 
                        {
                            sprintf_s(pname, NK_LEN(pname), "#%s", info->inputs[i].name);
                            it->consts[i] = nk_propertyf(ctx, pname, -100, it->consts[i], 100, 1, 1);
                        }
                    }
                    /* ====================================================*/
                    nk_group_end(ctx);
                }
                {
                    /* node connector and linking */
                    float space;
                    struct nk_rect bounds;
                    bounds = nk_layout_space_rect_to_local(ctx, node->bounds);
                    bounds.x += nodedit->scrolling.x;
                    bounds.y += nodedit->scrolling.y;
                    it->bounds = bounds;

                    /* output connector */
                    space = 29;
                    for (n = 0; n < infos[it->type].output_count; ++n) {
                        struct nk_rect circle;
                        circle.x = node->bounds.x + node->bounds.w-4;
                        circle.y = node->bounds.y + space * (float)n + node->header_height + space / 2;
                        circle.w = 8; circle.h = 8;
                        nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));

                        /* start linking process */
                        if (nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, circle, nk_true)) {
                            nodedit->linking.active = nk_true;
                            nodedit->linking.input_id = i;
                            nodedit->linking.input_slot = n;
                        }

                        /* draw curve from linked node slot to mouse position */
                        if (nodedit->linking.active && nodedit->linking.input_id == i &&
                            nodedit->linking.input_slot == n) {
                            struct nk_vec2 l0 = nk_vec2(circle.x + 3, circle.y + 3);
                            struct nk_vec2 l1 = in->mouse.pos;
                            nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
                                l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
                        }
                    }

                    /* input connector */
                    space = 29;
                    for (n = 0; n < infos[it->type].input_count; ++n) {
                        struct nk_rect circle;
                        int row = n + infos[it->type].output_count + infos[it->type].prop_count;
                        circle.x = node->bounds.x-4;
                        circle.y = node->bounds.y + space * (float)row + node->header_height + space / 2;
                        circle.w = 8; circle.h = 8;
                        nk_fill_circle(canvas, circle, nk_rgb(100, 100, 100));
                        if (nk_input_is_mouse_hovering_rect(in, circle))
                        {
                            struct node_link *link = find_node_input(it, n);
                            if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT) && !link &&
                                nodedit->linking.active && nodedit->linking.input_id != i) {
                                nodedit->linking.active = nk_false;
                                node_editor_link(nodedit, nodedit->linking.input_id,
                                    nodedit->linking.input_slot, i, n);
                                /* if link creates a cycle, remove it */
                                if (!tsort(nodedit->nodes, nodedit->node_count, NULL))
                                    node_editor_unlink(nodedit, nodedit->linking.input_id,
                                        nodedit->linking.input_slot, i, n);
                            }
                            if (nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) && link &&
                                !nodedit->linking.active) {
                                nodedit->linking.active = nk_true;
                                nodedit->linking.input_id = link->other_id;
                                nodedit->linking.input_slot = link->other_slot;
                                node_editor_unlink(nodedit, link->other_id, link->other_slot,
                                    i, link->slot);
                            }
                        }
                    }
                }
                {
                    /* draw node output links */
                    for (int i = 0; i < it->links.size; ++i) {
                        if (get_link(&it->links, i)->type == LINK_INBOUND)
                            continue;
                        struct node_link *link = get_link(&it->links, i);
                        struct node *ni = it;
                        struct node *no = &nodedit->nodes[link->other_id];
                        float spacei = 29;
                        float spaceo = 29;
                        int o_idx = link->other_slot + infos[no->type].output_count + infos[no->type].prop_count;
                        struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
                            nk_vec2(ni->bounds.x + ni->bounds.w, 3.0f + ni->bounds.y + spacei * (float)(link->slot) + 43));
                        struct nk_vec2 l1 = nk_layout_space_to_screen(ctx,
                            nk_vec2(no->bounds.x, 3.0f + no->bounds.y + spaceo * (float)(o_idx) + 43));

                        l0.x -= nodedit->scrolling.x;
                        l0.y -= nodedit->scrolling.y;
                        l1.x -= nodedit->scrolling.x;
                        l1.y -= nodedit->scrolling.y;

                        nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
                            l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
                    }
                }
            }

            /* reset linking connection */
            if (nodedit->linking.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT)) {
                nodedit->linking.active = nk_false;
            }

            /* TODO */
            if (updated != -1) {
                /* reshuffle nodes to have least recently selected node on top */
                /*struct node temp = nodedit->nodes[nodedit->node_count - 1];
                nodedit->nodes[nodedit->node_count - 1] = nodedit->nodes[updated];
                nodedit->nodes[updated] = temp;*/
            }

            /* node selection */
            if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT|NK_BUTTON_RIGHT, nk_layout_space_bounds(ctx))) {
                nodedit->selected_id = -1;
                nodedit->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200);
                for (int i = 0; i < nodedit->node_count; i++) {
                    it = &nodedit->nodes[i];
                    struct nk_rect b = nk_layout_space_rect_to_screen(ctx, it->bounds);
                    b.x -= nodedit->scrolling.x;
                    b.y -= nodedit->scrolling.y;
                    if (nk_input_is_mouse_hovering_rect(in, b))
                        nodedit->selected_id = i;
                }
            }

            /* contextual menu */
            if (nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx))) {
                nk_layout_row_dynamic(ctx, 25, 1);
                if (nodedit->selected_id != -1)
                {
                    if (nk_contextual_item_label(ctx, "delete", NK_TEXT_CENTERED))
                    {
                        node_editor_delete(nodedit, nodedit->selected_id);
                    }
                }
                else
                {
                    for (int i = 0; i < nodedit->conf->node_count; i++)
                    {
                        if (nk_contextual_item_label(ctx, infos[i].name, NK_TEXT_CENTERED))
                        {
                            struct nk_rect b = nk_layout_widget_bounds(ctx);
                            node_editor_add(nodedit, (node_type)i, b.x + nodedit->scrolling.x,
                                b.y + nodedit->scrolling.y);
                        }
                    }
                }
                if (nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
                    nk_contextual_close(ctx);
                nk_contextual_end(ctx);
            }
        }
        nk_layout_space_end(ctx);

        /* window content scrolling */
        if (nk_input_is_mouse_hovering_rect(in, nk_window_get_bounds(ctx)) &&
            nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE)) {
            nodedit->scrolling.x -= in->mouse.delta.x;
            nodedit->scrolling.y -= in->mouse.delta.y;
        }
    }
    nk_end(ctx);
    return !nk_window_is_closed(ctx, "NodeEdit");
}

/* depth-first topological sort */
static int 
tsort(struct node *nodes, int node_count, struct node **sorted)
{
    struct stack_frame
    {
        struct node *node;
        int loop_counter;
    };

    int result = nk_true;

    int sorted_count = 0;

    struct node **unmarked = malloc(node_count * sizeof *unmarked);
    int unmarked_count = 0;

    struct stack_frame *stack = malloc(node_count * sizeof *stack);
    int stack_size = 0;

    for (int i = 0; i < node_count; i++)
    {
        nodes[i].mark = MARK_NONE;
        unmarked[i] = &nodes[i];
    }

    unmarked_count = node_count;

    while (unmarked_count)
    {
        struct node *n = unmarked[0];

        stack[stack_size++] = (struct stack_frame) { n, 0 };

        while (stack_size)
        {
            struct stack_frame *s = &stack[stack_size - 1];
            n = s->node;

            if (n->mark == MARK_NONE)
            {
                for (int i = 0; i < unmarked_count; ++i)
                {
                    if (unmarked[i] == n)
                    {
                        unmarked[i] = unmarked[--unmarked_count];
                        break;
                    }
                }

                n->mark = MARK_TEMPORARY;
            }

            struct node *next = NULL;
            for (int i = s->loop_counter; i < n->links.size; ++i)
            {
                struct node_link *l = get_link(&n->links, i);
                if (l->type == LINK_OUTBOUND)
                {
                    next = &nodes[l->other_id];
                    /* if next->mark == MARK_TEMPORARY then the graph is not acyclic */
                    if (next->mark == MARK_TEMPORARY) { result = nk_false; goto cleanup; }
                    if (next->mark == MARK_NONE) { s->loop_counter = i + 1; break; }
                    next = NULL;
                }
            }

            if (next)
            {
                stack[stack_size++] = (struct stack_frame) { next, 0 };
                continue;
            }

            n->mark = MARK_PERMAMENT;
            if (sorted) sorted[sorted_count++] = n;
            --stack_size;
        }
    }

    if (sorted)
    {
        for (int i = 0; i < sorted_count / 2; ++i)
        {
            struct node *temp = sorted[i];
            sorted[i] = sorted[sorted_count - i - 1];
            sorted[sorted_count - i - 1] = temp;
        }
    }

    cleanup:
    free(stack);
    free(unmarked);

    return result;
}

static void
node_editor_save(struct node_editor *editor, char *path)
{
    SDL_RWops *file = SDL_RWFromFile(path, "w");
    size_t pos, temp;

    if (!file) { editor_print(editor, SDL_GetError()); return; }

    /* write magic */
    char *magic = "aigraph";
    SDL_RWwrite(file, magic, 1, strlen(magic) + 1);

    /* write scroll */
    SDL_RWwrite(file, &editor->scrolling, sizeof editor->scrolling, 1);

    /* write nodes */
    SDL_WriteLE32(file, editor->node_count);
    for (int i = 0; i < editor->node_count; ++i)
    {
        struct node *n = &editor->nodes[i];
        SDL_WriteLE16(file, (uint16_t)n->type);
        SDL_WriteLE32(file, *((uint32_t*)(&n->bounds.x)));
        SDL_WriteLE32(file, *((uint32_t*)(&n->bounds.y)));
    }

    /* reserve space for link count */
    pos = SDL_RWtell(file);
    SDL_WriteLE32(file, 0);

    /* write links */
    int link_count = 0;
    for (int i = 0; i < editor->node_count; ++i)
    {
        struct node *n = &editor->nodes[i];
        for (int j = 0; j < n->links.size; ++j)
        {
            struct node_link *link = get_link(&n->links, j);
            if (link->type == LINK_OUTBOUND)
            {
                SDL_WriteLE32(file, i);
                SDL_WriteLE32(file, link->other_id);
                SDL_WriteU8(file, (uint8_t)link->slot);
                SDL_WriteU8(file, (uint8_t)link->other_slot);
                ++link_count;
            }
        }
    }

    /* write link count */
    temp = SDL_RWtell(file);
    SDL_RWseek(file, pos, RW_SEEK_SET);
    SDL_WriteLE32(file, link_count);
    SDL_RWseek(file, temp, RW_SEEK_SET);

    /* reserve space for const input count */
    pos = SDL_RWtell(file);
    SDL_WriteLE32(file, 0);

    /* write const inputs */
    int const_inputs = 0;
    for (int i = 0; i < editor->node_count; ++i)
    {
        struct node *n = &editor->nodes[i];
        char is_const[16];
        memset(is_const, 1, NK_LEN(is_const));
        for (int j = 0; j < n->links.size; ++j)
        {
            struct node_link *link = get_link(&n->links, j);
            if (link->type == LINK_INBOUND) is_const[link->slot] = 0;
        }
        for (int j = 0; j < editor->conf->nodes[n->type].input_count; ++j)
        {
            if (is_const[j])
            {
                SDL_WriteLE32(file, i);
                SDL_WriteLE32(file, *((uint32_t*)(&n->consts[j])));
                SDL_WriteU8(file, (uint8_t)j);
                ++const_inputs;
            }
        }
    }

    /* write const input count */
    temp = SDL_RWtell(file);
    SDL_RWseek(file, pos, RW_SEEK_SET);
    SDL_WriteLE32(file, const_inputs);
    SDL_RWseek(file, temp, RW_SEEK_SET);

    /* reserve space for property count */
    pos = SDL_RWtell(file);
    SDL_WriteLE32(file, 0);

    /* write properties */
    int property_count = 0;
    for (int i = 0; i < editor->node_count; ++i)
    {
        struct node *n = &editor->nodes[i];
        struct node_info *info = &editor->conf->nodes[n->type];
        for (int j = 0; j < info->prop_count; ++j)
        {
            SDL_WriteLE32(file, i);
            SDL_WriteLE32(file, (uint32_t)n->props[j].i);
            SDL_WriteU8(file, (uint8_t)j);
            ++property_count;
        }
    }

    /* write property count */
    temp = SDL_RWtell(file);
    SDL_RWseek(file, pos, RW_SEEK_SET);
    SDL_WriteLE32(file, property_count);
    SDL_RWseek(file, temp, RW_SEEK_SET);

    if (SDL_RWclose(file) == -1)
        editor_print(editor, SDL_GetError());
    else
        editor_printf(editor, "successfully saved into file '%s'", path);
}

static void node_editor_clear(struct node_editor *editor)
{
    struct config *config = editor->conf;
    struct print_ops *console = editor->console;
    node_editor_cleanup(editor);
    node_editor_init(editor, config, console);
}

static void
node_editor_load(struct node_editor *editor, char *path)
{
#define READ(dst, size, n) do { if (!SDL_RWread(file, dst, size, n)) goto error; } while(0)
#define SEEK(offset, mode) do { if (SDL_RWseek(file, offset, mode) == -1) goto error; } while(0)
#define FAIL_IF(cond) do { if ((cond)) goto error; } while(0)

    struct node_data {
        node_type type;
        float x, y;
    };

    struct link_data {
        int input_id, output_id;
        int input_slot, output_slot;
    };

    struct const_data {
        int node_id;
        int node_slot;
        float value;
    };

    struct property_data {
        int node_id;
        int idx;
        int value;
    };

    SDL_RWops *file = SDL_RWFromFile(path, "r");
    struct nk_vec2 scroll;
    struct node_data *nodes = NULL;
    struct link_data *links = NULL;
    struct const_data *consts = NULL;
    struct property_data *props = NULL;

    if (!file) { editor_print(editor, SDL_GetError()); return; }

    /* read magic */
    char magic[8];
    READ(&magic, 1, NK_LEN(magic));
    if (strcmp(magic, "aigraph"))
    {
        editor_print(editor, "error: invalid file");
        goto cleanup;
    }

    /* read scroll */
    READ(&scroll, sizeof scroll, 1);

    /* read nodes */
    int node_count = (int)SDL_ReadLE32(file);
    FAIL_IF(node_count < 0 || node_count > 4096);
    if (node_count)
    {
        nodes = malloc(node_count * sizeof *nodes);
        for (int i = 0; i < node_count; ++i)
        {
            node_type type = (node_type)SDL_ReadLE16(file);
            uint32_t x_int = SDL_ReadLE32(file);
            uint32_t y_int = SDL_ReadLE32(file);
            FAIL_IF(type < 0 || type >= editor->conf->node_count);
            nodes[i].type = type;
            nodes[i].x = *((float*)(&x_int));
            nodes[i].y = *((float*)(&y_int));
        }
    }

    /* read links */
    int link_count = (int)SDL_ReadLE32(file);
    FAIL_IF(link_count < 0 || link_count > node_count * node_count);
    if (link_count)
    {
        links = malloc(link_count * sizeof *links);
        for (int i = 0; i < link_count; ++i)
        {
            int ii = (int)SDL_ReadLE32(file);
            int oi = (int)SDL_ReadLE32(file);
            int is = (int)SDL_ReadU8(file);
            int os = (int)SDL_ReadU8(file);
            FAIL_IF(ii < 0 || ii >= node_count);
            FAIL_IF(oi < 0 || oi >= node_count);
            FAIL_IF(is < 0);
            FAIL_IF(os < 0);
            links[i].input_id = ii;
            links[i].output_id = oi;
            links[i].input_slot = is;
            links[i].output_slot = os;
        }
    }

    /* read consts */
    int const_count = (int)SDL_ReadLE32(file);
    FAIL_IF(const_count < 0);
    if (const_count)
    {
        consts = malloc(const_count * sizeof *consts);
        for (int i = 0; i < const_count; ++i)
        {
            int id = (int)SDL_ReadLE32(file);
            uint32_t value = SDL_ReadLE32(file);
            int slot = (int)SDL_ReadU8(file);
            FAIL_IF(id < 0 || id >= node_count);
            FAIL_IF(slot < 0);
            consts[i].node_id = id;
            consts[i].node_slot = slot;
            consts[i].value = *((float*)(&value));
        }
    }

    /* read properties */
    int property_count = (int)SDL_ReadLE32(file);
    FAIL_IF(property_count < 0);
    if (property_count)
    {
        props = malloc(property_count * sizeof *props);
        for (int i = 0; i < property_count; ++i)
        {
            int id = (int)SDL_ReadLE32(file);
            int value = (int)SDL_ReadLE32(file);
            int idx = (int)SDL_ReadU8(file);
            FAIL_IF(id < 0 || id >= node_count);
            FAIL_IF(idx < 0);
            props[i].node_id = id;
            props[i].value = value;
            props[i].idx = idx;
        }
    }

    node_editor_clear(editor);

    /* TODO verify data */
    for (int i = 0; i < node_count; ++i)
        node_editor_add(editor, nodes[i].type, nodes[i].x, nodes[i].y);
    for (int i = 0; i < link_count; ++i)
        node_editor_link(editor, links[i].input_id, links[i].input_slot,
            links[i].output_id, links[i].output_slot);
    for (int i = 0; i < const_count; ++i)
        editor->nodes[consts[i].node_id].consts[consts[i].node_slot] =
            consts[i].value;
    for (int i = 0; i < property_count; ++i)
        editor->nodes[props[i].node_id].props[props[i].idx].i =
            props[i].value;
    editor->scrolling = scroll;

    editor_printf(editor, "file loaded: %d nodes, %d links, %d consts, %d properties",
        node_count, link_count, const_count, property_count);

    if (0) {
error:
        editor_print(editor, "error while reading file", path);
    }

cleanup:
    if (nodes) free(nodes);
    if (links) free(links);
    if (consts) free(consts);
    if (props) free(props);
    SDL_RWclose(file);
    return;

#undef READ
#undef SEEK
#undef FAIL_IF
}