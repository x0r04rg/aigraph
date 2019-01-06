
#include <SDL2/SDL_rwops.h>
#include "aigraph.h"

#define NODE_WIDTH 180.0f

enum link_type { LINK_INBOUND, LINK_OUTBOUND };
enum node_mark { MARK_NONE, MARK_TEMPORARY, MARK_PERMAMENT };

struct node_link {
    enum link_type type;
    int slot;
    int other_id;
    int other_slot;
};

struct node_link_list {
    size_t size, capacity;
    struct node_link *links;
};

struct node {
    node_type type;
    struct nk_rect bounds;
    struct node_link_list links;
    char field_names[10][MAX_INPUTS];
    float constant_inputs[MAX_INPUTS];
    enum node_mark mark; // needed for topological search
};

struct node_linking {
    int active;
    int input_id;
    int input_slot;
};

struct node_editor {
    struct node_info *infos;
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

static void
node_editor_add(struct node_editor *editor, node_type type, float pos_x, float pos_y)
{
    struct node_info *info = &editor->infos[type];
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
        max(info->input_count, info->output_count) + 35);
    for (int i = 0; i < info->input_count; i++)
        sprintf_s(node->field_names[i], 10, "#%s", info->inputs[i].name);
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
node_editor_init(struct node_editor *editor)
{
    memset(editor, 0, sizeof(*editor));
    editor->selected_id = -1;
    editor->infos = fill_infos();
    node_editor_add(editor, NODE_SUM, 40, 10);
}

static void
node_editor_cleanup(struct node_editor *editor)
{
    for (int i = 0; i < editor->node_count; ++i)
        if (editor->nodes[i].links.links)
            free(editor->nodes[i].links.links);
    free(editor->nodes);
    free(editor->infos);
}

static struct node_link*
find_node_input(struct node *node, int slot)
{
    struct node_link *link = NULL;
    for (int i = 0; i < node->links.size; ++i)
    {
        link = get_link(&node->links, i);
        if (link->type == LINK_INBOUND && link->slot == slot) break;
        link = NULL;
    }
    return link;
}

static struct compiled_graph* node_editor_compile(struct node_editor *editor);

static int
node_editor(struct nk_context *ctx, struct node_editor *nodedit, struct nk_rect win_size, 
    nk_flags flags)
{
    int n = 0;
    struct nk_rect total_space;
    const struct nk_input *in = &ctx->input;
    struct nk_command_buffer *canvas;
    int updated = -1;
    struct node_info *infos = nodedit->infos;

    if (nk_begin(ctx, "NodeEdit", win_size, flags))
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
                    nk_layout_row_dynamic(ctx, 25, 2);
                    struct node_info info = infos[it->type];
                    for (int i = 0; i < max(info.input_count, info.output_count); i++)
                    {
                        if (i < info.input_count)
                        {
                            if (find_node_input(it, i)) nk_label(ctx, info.inputs[i].name, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                            else it->constant_inputs[i] = nk_propertyf(ctx, it->field_names[i], -INFINITY, it->constant_inputs[i], INFINITY, 1, 1);
                        }
                        else
                            nk_label(ctx, "", 0);

                        if (i < info.output_count)
                            nk_label(ctx, info.outputs[i].name, NK_TEXT_ALIGN_RIGHT | NK_TEXT_ALIGN_MIDDLE);
                        else
                            nk_label(ctx, "", 0);
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
                        circle.y = node->bounds.y + space * (float)n + node->header_height + 14;
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
                        circle.x = node->bounds.x-4;
                        circle.y = node->bounds.y + space * (float)n + node->header_height + 14;
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
                        struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
                            nk_vec2(ni->bounds.x + ni->bounds.w, 3.0f + ni->bounds.y + spacei * (float)(link->slot) + 43));
                        struct nk_vec2 l1 = nk_layout_space_to_screen(ctx,
                            nk_vec2(no->bounds.x, 3.0f + no->bounds.y + spaceo * (float)(link->other_slot) + 43));

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
                    if (nk_contextual_item_label(ctx, "_compile_", NK_TEXT_CENTERED))
                    {
                        /*struct compiled_graph *result = node_editor_compile(nodedit);
                        free(result);*/
                    }
                    for (int i = 0; i < NODE_TYPE_COUNT; i++)
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

/* TODO */
#if 0
static struct compiled_graph*
node_editor_compile(struct node_editor *editor)
{
    struct node **sorted = malloc(editor->node_count * sizeof(struct node*));

    /* depth-first topological sort */
    {
        size_t sorted_count = 0;

        struct node **unmarked = malloc(editor->node_count * sizeof(struct node*));
        size_t unmarked_count = 0;

        struct node **stack = malloc(editor->node_count * sizeof(struct node*));
        size_t stack_size = 0, stack_capacity = editor->node_count;

        for (int i = 0; i < editor->node_count; i++)
        {
            editor->nodes[i].mark = MARK_NONE;
            unmarked[i] = &editor->nodes[i];
        }

        unmarked_count = editor->node_count;

        while (unmarked_count)
        {
            struct node *n = unmarked[0];

            NK_ASSERT(stack_size <= stack_capacity);
            stack[stack_size++] = n;

            while (stack_size)
            {
                n = stack[stack_size - 1];

                for (int i = 0; i < unmarked_count; ++i)
                {
                    if (unmarked[i] == n)
                    {
                        unmarked[i] = unmarked[--unmarked_count];
                        break;
                    }
                }

                n->mark = MARK_TEMPORARY;

                struct node *next = NULL;
                for (int i = 0; i < editor->link_count; ++i)
                {
                    struct node_link *l = &editor->links[i];
                    if (l->input_id == n->ID)
                    {
                        struct node *c = node_editor_find(editor, l->output_id);
                        /* if c->mark == MARK_TEMPORARY then the graph is not acyclic */
                        NK_ASSERT(c->mark != MARK_TEMPORARY);
                        if (c->mark == MARK_NONE)
                        {
                            next = c;
                            break;
                        }
                    }
                }

                if (next)
                {
                    NK_ASSERT(stack_size <= stack_capacity);
                    stack[stack_size++] = next;
                    continue;
                }

                n->mark = MARK_PERMAMENT;
                sorted[sorted_count++] = n;
                --stack_size;
            }
        }

        free(stack);
        free(unmarked);

        for (int i = 0; i < sorted_count / 2; ++i)
        {
            struct node *temp = sorted[i];
            sorted[i] = sorted[sorted_count - i - 1];
            sorted[sorted_count - i - 1] = temp;
        }
    }

    /* compile graph into binary blob */
    {
#define PAD_TO_ALIGN(pos, align) do { size_t p = (pos / align) * align; if (p != pos) pos = p + align; } while(0)

        struct node_info *infos = editor->infos;
        char *data;
        size_t pos = 0;
        const int align = 8;

        /* size prepass */
        pos += sizeof(struct compiled_graph);
        PAD_TO_ALIGN(pos, align);
        for (int i = 0; i < editor->node_count; i++)
        {
            struct node *n = sorted[i];

            for (int j = 0; j < infos[n->type].input_count; j++)
                if (!isnan(n->constant_inputs[j])) pos += sizeof(float);

            PAD_TO_ALIGN(pos, align);

            pos += infos[n->type].size;

            PAD_TO_ALIGN(pos, align);
        }

        /* write constant inputs, remember offsets */
        data = malloc(pos);
        memset(data, 0, pos);
        ((struct compiled_graph*)data)->size = pos;
        pos = 0;
        size_t *offsets = malloc(editor->node_count * sizeof *offsets);

        pos += sizeof(struct compiled_graph);
        PAD_TO_ALIGN(pos, align);
        for (int i = 0; i < editor->node_count; ++i)
        {
            struct node *n = sorted[i];
            struct node_info *info = &infos[n->type];
            size_t input_offsets[MAX_INPUTS];

            for (int j = 0; j < info->input_count; ++j)
            {
                if (!isnan(n->constant_inputs[j]))
                {
                    *((float*)(data + pos)) = n->constant_inputs[j];
                    input_offsets[j] = pos;
                    pos += sizeof(float);
                }
            }

            PAD_TO_ALIGN(pos, align);

            offsets[i] = pos;
            ((struct node_base*)(data + pos))->type = n->type;

            for (int j = 0; j < info->input_count; ++j)
            {
                if (!isnan(n->constant_inputs[j]))
                {
                    *((float**)(data + pos + info->inputs[j].offset)) = input_offsets[j];
                }
            }

            pos += infos[n->type].size;

            PAD_TO_ALIGN(pos, align);
        }

        /* write node offsets */
        ((struct compiled_graph*)data)->first = (struct node_base*)offsets[0];
        for (int i = 0; i < editor->node_count - 1; ++i)
            ((struct node_base*)(data + offsets[i]))->next = (struct node_base*)offsets[i + 1];

        /* write input offsets */
        for (int i = 0; i < editor->link_count; ++i)
        {
            struct node_link *link = &editor->links[i];
            size_t in = 0, out = 0;

            for (int i = 0; i < editor->node_count; ++i)
            {
                node_type type = ((struct node_base*)(data + offsets[i]))->type;
                if (sorted[i]->ID == link->input_id)
                {
                    in = offsets[i] + infos[type].outputs[link->input_slot].offset;
                    if (out) break;
                }
                if (sorted[i]->ID == link->output_id)
                {
                    out = offsets[i] + infos[type].inputs[link->output_slot].offset;
                    if (in) break;
                }
            }

            NK_ASSERT(in && out);

            *((float**)(data + out)) = (float*)(in);
        }

        free(offsets);
        free(sorted);

        return (struct compiled_graph*)data;

#undef PAD_TO_ALIGN
    }
}

#endif