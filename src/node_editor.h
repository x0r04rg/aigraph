
#include <SDL2/SDL_rwops.h>
#include "aigraph.h"

#define NODE_WIDTH 180.0f

struct node {
    int ID;
    node_type type;
    struct nk_rect bounds;
    float constant_inputs[MAX_INPUTS]; // NaN means not constant
};

struct node_link {
    int input_id;
    int input_slot;
    int output_id;
    int output_slot;
    struct nk_vec2 in;
    struct nk_vec2 out;
};

struct node_linking {
    int active;
    int input_id;
    int input_slot;
};

struct node_editor {
    int IDs;
    struct node_info *infos;
    struct node *nodes;
    struct node_link *links;
    size_t node_count, nodes_capacity;
    size_t link_count, links_capacity;
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

static struct node*
node_editor_find(struct node_editor *editor, int ID)
{
    for (int i = 0; i < editor->node_count; i++)
    {
        if (editor->nodes[i].ID == ID)
            return &editor->nodes[i];
    }
    return NULL;
}

static void
node_editor_add(struct node_editor *editor, node_type type, float pos_x, float pos_y)
{
    struct node_info *infos = editor->infos;
    struct node *node;
    if (editor->node_count == editor->nodes_capacity)
    {
        size_t new_capacity = editor->nodes_capacity ? 2 * editor->nodes_capacity : 10;
        editor->nodes = realloc(editor->nodes, new_capacity * sizeof(struct node));
        editor->nodes_capacity = new_capacity;
    }
    node = &editor->nodes[editor->node_count++];
    memset(node, 0, sizeof *node);
    node->ID = editor->IDs++;
    node->type = type;
    node->bounds = nk_rect(pos_x, pos_y, NODE_WIDTH, 30 * 
        max(infos[type].input_count, infos[type].output_count) + 35);
}

static void 
node_editor_delete(struct node_editor *editor, int node_id)
{
    int i = (int)(node_editor_find(editor, node_id) - editor->nodes);
    editor->nodes[i] = editor->nodes[--editor->node_count];
    for (i = 0; i < editor->link_count;)
    {
        struct node_link *link = &editor->links[i];
        if (link->input_id == node_id || link->output_id == node_id)
            editor->links[i] = editor->links[--editor->link_count];
        else i++;
    }
}

static int
node_editor_find_link_by_output(struct node_editor *editor, int out_id, int out_slot)
{
    for (int i = 0; i < editor->link_count; i++)
    {
        if (editor->links[i].output_id == out_id && editor->links[i].output_slot == out_slot)
            return i;
    }
    return -1;
}

static void
node_editor_link(struct node_editor *editor, int in_id, int in_slot,
    int out_id, int out_slot)
{
    struct node_link *link;
    if (editor->link_count == editor->links_capacity)
    {
        size_t new_capacity = editor->links_capacity ? 2 * editor->links_capacity : 30;
        editor->links = realloc(editor->links, new_capacity * sizeof(struct node_link));
        editor->links_capacity = new_capacity;
    }
    link = &editor->links[editor->link_count++];
    link->input_id = in_id;
    link->input_slot = in_slot;
    link->output_id = out_id;
    link->output_slot = out_slot;
    node_editor_find(editor, out_id)->constant_inputs[out_slot] = NAN;
}

static void
node_editor_unlink(struct node_editor *editor, int idx)
{
    int out_id = editor->links[idx].output_id;
    int out_slot = editor->links[idx].output_slot;
    node_editor_find(editor, out_id)->constant_inputs[out_slot] = 0;
    editor->links[idx] = editor->links[--editor->link_count];
}

static void
node_editor_init(struct node_editor *editor)
{
    memset(editor, 0, sizeof(*editor));
    editor->IDs = 1;
    editor->infos = fill_infos();
    node_editor_add(editor, NODE_SUM, 40, 10);
}

static void
node_editor_cleanup(struct node_editor *editor)
{
    free(editor->infos);
    free(editor->nodes);
    free(editor->links);
}

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
                            if (isnan(it->constant_inputs[i])) nk_label(ctx, info.inputs[i].name, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
                            else it->constant_inputs[i] = nk_propertyf(ctx, info.inputs[i].name, -INFINITY, it->constant_inputs[i], INFINITY, 0.01f, 1);
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
                            nodedit->linking.input_id = it->ID;
                            nodedit->linking.input_slot = n;
                        }

                        /* draw curve from linked node slot to mouse position */
                        if (nodedit->linking.active && nodedit->linking.input_id == it->ID &&
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
                            if (nk_input_is_mouse_released(in, NK_BUTTON_LEFT) &&
                                node_editor_find_link_by_output(nodedit, it->ID, n) == -1 &&
                                nodedit->linking.active && nodedit->linking.input_id != it->ID) {
                                nodedit->linking.active = nk_false;
                                node_editor_link(nodedit, nodedit->linking.input_id,
                                    nodedit->linking.input_slot, it->ID, n);
                            }
                            if (nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) &&
                                !nodedit->linking.active) {
                                int i = node_editor_find_link_by_output(nodedit, it->ID, n);
                                if (i != -1)
                                {
                                    struct node_link link = nodedit->links[i];
                                    node_editor_unlink(nodedit, i);
                                    nodedit->linking.active = nk_true;
                                    nodedit->linking.input_id = link.input_id;
                                    nodedit->linking.input_slot = link.input_slot;
                                }
                            }
                        }
                    }
                }
            }

            /* reset linking connection */
            if (nodedit->linking.active && nk_input_is_mouse_released(in, NK_BUTTON_LEFT)) {
                nodedit->linking.active = nk_false;
            }

            /* draw each link */
            for (n = 0; n < nodedit->link_count; ++n) {
                struct node_link *link = &nodedit->links[n];
                struct node *ni = node_editor_find(nodedit, link->input_id);
                struct node *no = node_editor_find(nodedit, link->output_id);
                float spacei = 29;
                float spaceo = 29;
                struct nk_vec2 l0 = nk_layout_space_to_screen(ctx,
                    nk_vec2(ni->bounds.x + ni->bounds.w, 3.0f + ni->bounds.y + spacei * (float)(link->input_slot) + 43));
                struct nk_vec2 l1 = nk_layout_space_to_screen(ctx,
                    nk_vec2(no->bounds.x, 3.0f + no->bounds.y + spaceo * (float)(link->output_slot) + 43));

                l0.x -= nodedit->scrolling.x;
                l0.y -= nodedit->scrolling.y;
                l1.x -= nodedit->scrolling.x;
                l1.y -= nodedit->scrolling.y;

                nk_stroke_curve(canvas, l0.x, l0.y, l0.x + 50.0f, l0.y,
                    l1.x - 50.0f, l1.y, l1.x, l1.y, 1.0f, nk_rgb(100, 100, 100));
            }

            if (updated != -1) {
                /* reshuffle nodes to have least recently selected node on top */
                struct node temp = nodedit->nodes[nodedit->node_count - 1];
                nodedit->nodes[nodedit->node_count - 1] = nodedit->nodes[updated];
                nodedit->nodes[updated] = temp;
            }

            /* node selection */
            if (nk_input_mouse_clicked(in, NK_BUTTON_LEFT|NK_BUTTON_RIGHT, nk_layout_space_bounds(ctx))) {
                nodedit->selected_id = 0;
                nodedit->bounds = nk_rect(in->mouse.pos.x, in->mouse.pos.y, 100, 200);
                for (int i = 0; i < nodedit->node_count; i++) {
                    it = &nodedit->nodes[i];
                    struct nk_rect b = nk_layout_space_rect_to_screen(ctx, it->bounds);
                    b.x -= nodedit->scrolling.x;
                    b.y -= nodedit->scrolling.y;
                    if (nk_input_is_mouse_hovering_rect(in, b))
                        nodedit->selected_id = it->ID;
                }
            }

            /* contextual menu */
            if (nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx))) {
                nk_layout_row_dynamic(ctx, 25, 1);
                if (nodedit->selected_id)
                {
                    if (nk_contextual_item_label(ctx, "delete", NK_TEXT_CENTERED))
                    {
                        node_editor_delete(nodedit, nodedit->selected_id);
                    }
                }
                else
                {
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

