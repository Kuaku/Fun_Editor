#include "./tree.h"

void TreeHolderInit(TreeHolder* holder, size_t initial_size) {
    if (initial_size == 0) initial_size = 1;
    holder->nodes = (RenderNode*)calloc(initial_size, sizeof(RenderNode));
    holder->node_capacity = initial_size;
    holder->node_size = 0;
    TraversalOrderInit(&holder->traversal_list, initial_size);
}

void TreeHolderClear(TreeHolder* holder) {
    TraversalOrderClear(&holder->traversal_list);
    free(holder->nodes);
    holder->nodes = NULL;
    holder->node_capacity = 0;
    holder->node_size = 0;
}

void TreeReset(TreeHolder* holder) {
    holder->node_size = 0;
}

void ResetNode(TreeHolder* holder, NodeHandle node) {
    memset(&holder->nodes[node], 0, sizeof(RenderNode));
    holder->nodes[node].parent = INVALID_NODE;
    holder->nodes[node].first_child = INVALID_NODE;
    holder->nodes[node].last_child = INVALID_NODE;
    holder->nodes[node].previous_sibling = INVALID_NODE;
    holder->nodes[node].next_sibling = INVALID_NODE;
}

void AppendChild(TreeHolder* holder, NodeHandle parent, NodeHandle child) {
    RenderNode* parent_node = &holder->nodes[parent];
    RenderNode* child_node  = &holder->nodes[child];

    child_node->parent = parent;
    child_node->next_sibling = INVALID_NODE;

    if (parent_node->last_child == INVALID_NODE) {
        parent_node->first_child = child;
        child_node->previous_sibling = INVALID_NODE;
    } else {
        holder->nodes[parent_node->last_child].next_sibling = child;
        child_node->previous_sibling = parent_node->last_child;
    }
    parent_node->last_child = child;
}

void CalculateBounds(Editor* editor, TreeHolder* holder) {
    for (NodeHandle i = 0; i < (NodeHandle)holder->node_size; i++) {
        if (holder->nodes[i].parent == INVALID_NODE) {
            CalculateBoundsSubTree(editor, holder, i);
        }
    }
}

void CalculateBoundsSubTree(Editor* editor, TreeHolder* holder, NodeHandle root) {
    TraversalOrderReset(&holder->traversal_list);
    BuildTraversalOrder(holder, root, &holder->traversal_list);

    for (int i = (int)holder->traversal_list.count - 1; i >= 0; i--) {
        RenderNodeMeasure(holder, holder->traversal_list.order[i]);
    }

    for (size_t i = 0; i < holder->traversal_list.count; i++) {
        RenderNodePlace(editor, holder, holder->traversal_list.order[i]);
    }
}

void TraversalOrderInit(TraversalOrder* t, size_t initial) {
    if (initial == 0) initial = 1;
    t->order = (NodeHandle*)malloc(initial * sizeof(NodeHandle));
    t->count = 0;
    t->capacity = initial;
}

void TraversalOrderClear(TraversalOrder* t) {
    free(t->order);
    t->order = NULL;
    t->count = 0;
    t->capacity = 0;
}

void TraversalOrderReset(TraversalOrder* t) {
    t->count = 0;
}

void TraversalOrderPush(TraversalOrder* t, NodeHandle h) {
    while (t->count >= t->capacity) {
        t->capacity = t->capacity ? t->capacity * 2 : 64;
        t->order = (NodeHandle*)realloc(t->order, t->capacity * sizeof(NodeHandle));
    }
    t->order[t->count++] = h;
}

void BuildTraversalOrder(TreeHolder* holder, NodeHandle root, TraversalOrder* out) {
    out->count = 0;
    if (root == INVALID_NODE) return;

    size_t stack_cap = holder->node_size > 0 ? holder->node_size : 1;
    NodeHandle* stack = (NodeHandle*)malloc(stack_cap * sizeof(NodeHandle));
    size_t stack_count = 0;

    stack[stack_count++] = root;

    while (stack_count > 0) {
        NodeHandle current = stack[--stack_count];
        TraversalOrderPush(out, current);
        NodeHandle child = holder->nodes[current].last_child;
        while (child != INVALID_NODE) {
            if (stack_count == stack_cap) {
                stack_cap *= 2;
                stack = (NodeHandle*)realloc(stack, stack_cap * sizeof(NodeHandle));
            }
            stack[stack_count++] = child;
            child = holder->nodes[child].previous_sibling;
        }
    }
    free(stack);
}

void RenderNodeMeasure(TreeHolder* holder, NodeHandle node) {
    RenderNode* render_node = &holder->nodes[node];

    if (render_node->horizontal_axis_type == AXIS_FIXED) {
        render_node->border.size.x = render_node->fixed_size.x;
        render_node->outer_bounds.size.x = render_node->border.size.x + render_node->margin.position.x + render_node->margin.size.x;
    }

    if (render_node->vertical_axis_type == AXIS_FIXED) {
        render_node->border.size.y = render_node->fixed_size.y;
        render_node->outer_bounds.size.y = render_node->border.size.y + render_node->margin.position.y + render_node->margin.size.y;
    }
}

void RenderNodeCalculateContent(TreeHolder* holder, NodeHandle node) {
    RenderNode* render_node = &holder->nodes[node];

    render_node->inner_bounds.size.x = render_node->border.size.x - (render_node->padding.position.x + render_node->padding.size.x);
    render_node->inner_bounds.size.y = render_node->border.size.y - (render_node->padding.position.y + render_node->padding.size.y);
    if (render_node->inner_bounds.size.x < 0) render_node->inner_bounds.size.x = 0;
    if (render_node->inner_bounds.size.y < 0) render_node->inner_bounds.size.y = 0;

    render_node->inner_bounds.position.x = render_node->border.position.x + render_node->padding.position.x;
    render_node->inner_bounds.position.y = render_node->border.position.y + render_node->padding.position.y;
}

void RenderNodePlace(Editor* editor, TreeHolder* holder, NodeHandle node) {
    RenderNodeCalculateContent(holder, node);
    RenderNode* render_node = &holder->nodes[node];

    if (render_node->layout == LAYOUT_NONE) return;

    bool is_vertical = render_node->layout == LAYOUT_VERTICAL;

    int grow_count = 0;
    int taken = 0;
    NodeHandle child = render_node->first_child;
    while (child != INVALID_NODE) {
        RenderNode* child_node = &holder->nodes[child];
        NodeHandle next = child_node->next_sibling;

        if (child_node->position_type == POSITION_FIXED) {
            child = next;
            continue;
        }

        if (child_node->horizontal_axis_type == AXIS_MEASURE_SECOND) {
            int w = child_node->custom_measure_x
                  ? child_node->custom_measure_x(editor, render_node->inner_bounds) : 0;
            child_node->border.size.x       = w;
            child_node->outer_bounds.size.x = w + child_node->margin.position.x + child_node->margin.size.x;
        }
        if (child_node->vertical_axis_type == AXIS_MEASURE_SECOND) {
            int h = child_node->custom_measure_y
                  ? child_node->custom_measure_y(editor, render_node->inner_bounds) : 0;
            child_node->border.size.y       = h;
            child_node->outer_bounds.size.y = h + child_node->margin.position.y + child_node->margin.size.y;
        }

        AxisType relevant_axis = is_vertical ? child_node->vertical_axis_type
                                             : child_node->horizontal_axis_type;
        int along = is_vertical ? child_node->outer_bounds.size.y
                                : child_node->outer_bounds.size.x;

        if (relevant_axis == AXIS_GROW) {
            grow_count++;
            taken += is_vertical ? child_node->margin.position.y + child_node->margin.size.y
                                 : child_node->margin.position.x + child_node->margin.size.x;
        } else {
            taken += along;
        }
        child = next;
    }

    int inner_along  = is_vertical ? render_node->inner_bounds.size.y
                                   : render_node->inner_bounds.size.x;
    int inner_cross  = is_vertical ? render_node->inner_bounds.size.x
                                   : render_node->inner_bounds.size.y;
    int grow_share   = (grow_count > 0) ? (inner_along - taken) / grow_count : 0;
    if (grow_share < 0) grow_share = 0;

    int cursor       = is_vertical ? render_node->inner_bounds.position.y
                                   : render_node->inner_bounds.position.x;
    int cross_origin = is_vertical ? render_node->inner_bounds.position.x
                                   : render_node->inner_bounds.position.y;

    child = render_node->first_child;
    while (child != INVALID_NODE) {
        RenderNode* c = &holder->nodes[child];
        NodeHandle next = c->next_sibling;

        if (c->position_type == POSITION_FIXED) {
            c->border.position.x = render_node->inner_bounds.position.x + c->fixed_position.x;
            c->border.position.y = render_node->inner_bounds.position.y + c->fixed_position.y;
            c->outer_bounds.position.x = c->border.position.x - c->margin.position.x;
            c->outer_bounds.position.y = c->border.position.y - c->margin.position.y;
            c->outer_bounds.size.x = c->border.size.x + c->margin.position.x + c->margin.size.x;
            c->outer_bounds.size.y = c->border.size.y + c->margin.position.y + c->margin.size.y;
            child = next;
            continue;
        }

        AxisType along_axis = is_vertical ? c->vertical_axis_type : c->horizontal_axis_type;
        AxisType cross_axis = is_vertical ? c->horizontal_axis_type : c->vertical_axis_type;

        int along_lead  = is_vertical ? c->margin.position.y : c->margin.position.x;
        int along_trail = is_vertical ? c->margin.size.y     : c->margin.size.x;
        int cross_lead  = is_vertical ? c->margin.position.x : c->margin.position.y;
        int cross_trail = is_vertical ? c->margin.size.x     : c->margin.size.y;

        int along_border;
        if (along_axis == AXIS_GROW) {
            along_border = grow_share - (along_lead + along_trail);
            if (along_border < 0) along_border = 0;
        } else {
            along_border = is_vertical ? c->border.size.y : c->border.size.x;
        }

        int cross_border;
        if (cross_axis == AXIS_GROW) {
            cross_border = inner_cross - (cross_lead + cross_trail);
            if (cross_border < 0) cross_border = 0;
        } else {
            cross_border = is_vertical ? c->border.size.x : c->border.size.y;
        }

        if (is_vertical) {
            c->border.size.y = along_border;
            c->border.size.x = cross_border;
            c->border.position.y = cursor + along_lead;
            c->border.position.x = cross_origin + cross_lead;
        } else {
            c->border.size.x = along_border;
            c->border.size.y = cross_border;
            c->border.position.x = cursor + along_lead;
            c->border.position.y = cross_origin + cross_lead;
        }

        c->outer_bounds.position.x = c->border.position.x - c->margin.position.x;
        c->outer_bounds.position.y = c->border.position.y - c->margin.position.y;
        c->outer_bounds.size.x = c->border.size.x + c->margin.position.x + c->margin.size.x;
        c->outer_bounds.size.y = c->border.size.y + c->margin.position.y + c->margin.size.y;

        cursor += is_vertical ? c->outer_bounds.size.y : c->outer_bounds.size.x;

        child = next;
    }
}

NodeHandle CreateNode(TreeHolder* holder) {
    if (holder->node_capacity == 0) {
        holder->node_capacity = 1;
        holder->nodes = (RenderNode*)realloc(holder->nodes,
                            holder->node_capacity * sizeof(RenderNode));
    }
    while (holder->node_size >= holder->node_capacity) {
        holder->node_capacity *= 2;
        holder->nodes = (RenderNode*)realloc(holder->nodes,
                            holder->node_capacity * sizeof(RenderNode));
    }
    NodeHandle out = (NodeHandle)holder->node_size++;
    ResetNode(holder, out);
    return out;
}

NodeHandle NodeCreateConfigured(TreeHolder* holder, RenderNode config) {
    NodeHandle h = CreateNode(holder);
    RenderNode* n = &holder->nodes[h];
    NodeHandle parent = n->parent,
               first = n->first_child,
               last  = n->last_child,
               prev  = n->previous_sibling,
               next  = n->next_sibling;
    *n = config;
    n->parent = parent;   
    n->first_child = first;
    n->last_child = last;
    n->previous_sibling = prev;
    n->next_sibling = next;
    return h;
}