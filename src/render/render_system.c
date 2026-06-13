#include "render_system.h"
#include "../editor/editor.h"

RenderSystem InitRenderSystem(RenderWrapper wrapper) {
    RenderSystem out = {0};
    out.render_wrapper = wrapper;
    TreeHolderInit(&out.tree_holder, INITIAL_RENDER_TREE_CAPACITY);
    out.render_queue = InitRenderQueue(INITIAL_RENDER_QUEUE_CAPACITY);
    return out;
}

void ClearRenderSystem(RenderSystem* system) {
    ClearRenderQueue(&system->render_queue);
    TreeHolderClear(&system->tree_holder);
    system->render_wrapper.destructor(&system->render_wrapper);
}

void CalculateSizingModel(Editor* editor, RenderSystem* system) {
    CalculateBounds(editor, &system->tree_holder);
}

static int RenderEntryCompare(const void* a, const void* b) {
    const RenderEntry* ea = (const RenderEntry*)a;
    const RenderEntry* eb = (const RenderEntry*)b;
    if (ea->z_index != eb->z_index)
        return (ea->z_index > eb->z_index) - (ea->z_index < eb->z_index);
    return (ea->order > eb->order) - (ea->order < eb->order);
}

void GenerateRenderList(RenderSystem* system) {
    TreeHolder* holder = &system->tree_holder;
    TraversalOrder* out = &holder->traversal_list;
    out->count = 0;

    if (holder->node_size == 0) return;

    size_t stack_cap = holder->node_size;
    NodeHandle* stack = (NodeHandle*)malloc(stack_cap * sizeof(NodeHandle));
    size_t stack_count = 0;

    RenderEntry* roots = (RenderEntry*)malloc(holder->node_size * sizeof(RenderEntry));
    size_t root_count = 0;
    for (NodeHandle i = 0; i < (NodeHandle)holder->node_size; i++) {
        if (holder->nodes[i].parent == INVALID_NODE) {
            roots[root_count].handle = i;
            roots[root_count].z_index = holder->nodes[i].z_index;
            roots[root_count].order = root_count;
            root_count++;
        }
    }
    qsort(roots, root_count, sizeof(RenderEntry), RenderEntryCompare);

    for (size_t i = root_count; i > 0; i--) {
        stack[stack_count++] = roots[i - 1].handle;
    }
    free(roots);

    RenderEntry* children = NULL;
    size_t children_cap = 0;

    while (stack_count > 0) {
        NodeHandle current = stack[--stack_count];
        TraversalOrderPush(out, current); // emit parent first

        size_t child_count = 0;
        NodeHandle c = holder->nodes[current].first_child;
        while (c != INVALID_NODE) {
            if (child_count >= children_cap) {
                children_cap = children_cap ? children_cap * 2 : 16;
                children = (RenderEntry*)realloc(children, children_cap * sizeof(RenderEntry));
            }
            children[child_count].handle = c;
            children[child_count].z_index = holder->nodes[c].z_index;
            children[child_count].order = child_count;
            child_count++;
            c = holder->nodes[c].next_sibling;
        }

        qsort(children, child_count, sizeof(RenderEntry), RenderEntryCompare);

        while (stack_count + child_count > stack_cap) {
            stack_cap *= 2;
            stack = (NodeHandle*)realloc(stack, stack_cap * sizeof(NodeHandle));
        }

        for (size_t i = child_count; i > 0; i--) {
            stack[stack_count++] = children[i - 1].handle;
        }
    }

    free(children);
    free(stack);
}

void BuildEditorRenderTree(Editor* editor) {
    RenderSystem* system = &editor->render_system;
    RenderWrapper* wrapper = &system->render_wrapper;
    Position screen_size = {
        wrapper->get_screen_width(wrapper),
        wrapper->get_screen_height(wrapper)
    };

    NodeHandle root = VNODE(system, .horizontal_axis_type = AXIS_FIXED, .vertical_axis_type = AXIS_FIXED, .fixed_size = screen_size);
    NodeHandle header = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_FIXED, .fixed_size = {editor->settings.font_size, 0}, .padding = {{5, 5}, {5, 5}});
    NodeHandle main_editor = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_GROW);
    NodeHandle footer = HNODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_FIXED, .fixed_size = {editor->settings.font_size, 0}, .padding = {{5, 5}, {5, 5}});

    AppendChild(&system->tree_holder, root, header);
    AppendChild(&system->tree_holder, root, main_editor);
    AppendChild(&system->tree_holder, root, footer);
}
