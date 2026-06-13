#ifndef TREE_H
#define TREE_H
#include "../common.h"

typedef enum AxisType {
    AXIS_FIXED = 0,
    AXIS_GROW,
    AXIS_MEASURE_SECOND
} AxisType;

typedef enum PositionType {
    POSITION_DEPENDENT = 0,
    POSITION_FIXED
} PositionType;

typedef enum Layout {
    LAYOUT_NONE = 0,
    LAYOUT_VERTICAL,
    LAYOUT_HORIZONTAL
} Layout;

typedef int NodeHandle;
#define INVALID_NODE -1

typedef struct RenderNode RenderNode;
typedef void (*RenderFunc)(Editor* editor, RenderNode* self);
typedef int (*MeasureFunc)(Editor* editor, Rect parent_size);

typedef struct RenderNode {
    Layout layout;
    PositionType position_type;
    AxisType vertical_axis_type;
    AxisType horizontal_axis_type;

    Position fixed_position;
    Position fixed_size;
    Rect padding;
    Rect margin;

    Rect outer_bounds;
    Rect border;
    Rect inner_bounds;

    int z_index;

    NodeHandle parent;

    NodeHandle first_child;
    NodeHandle last_child;
    NodeHandle next_sibling;
    NodeHandle previous_sibling;

    RenderFunc custom_render;
    MeasureFunc custom_measure_x;
    MeasureFunc custom_measure_y;
} RenderNode;

typedef struct {
    NodeHandle* order;
    size_t count;
    size_t capacity;
} TraversalOrder;

typedef struct TreeHolder {
    RenderNode* nodes;
    size_t node_capacity;
    size_t node_size;
    TraversalOrder traversal_list;
} TreeHolder;

void TreeHolderInit(TreeHolder* holder, size_t initial_size);
void TreeHolderClear(TreeHolder* holder);
void TreeReset(TreeHolder* holder);
void ResetNode(TreeHolder* holder, NodeHandle node);
void AppendChild(TreeHolder* holder, NodeHandle parent, NodeHandle child);
void CalculateBounds(Editor* editor, TreeHolder* holder);
void CalculateBoundsSubTree(Editor* editor, TreeHolder* holder, NodeHandle root);

void TraversalOrderInit(TraversalOrder* t, size_t initial);
void TraversalOrderClear(TraversalOrder* t);
void TraversalOrderReset(TraversalOrder* t);
void TraversalOrderPush(TraversalOrder* t, NodeHandle h);
void BuildTraversalOrder(TreeHolder* holder, NodeHandle root, TraversalOrder* out);

void RenderNodeMeasure(TreeHolder* holder, NodeHandle node);
void RenderNodePlace(Editor* editor, TreeHolder* holder, NodeHandle node);
void RenderNodeCalculateContent(TreeHolder* holder, NodeHandle node);

NodeHandle CreateNode(TreeHolder* holder);
NodeHandle NodeCreateConfigured(TreeHolder* holder, RenderNode config);

#endif