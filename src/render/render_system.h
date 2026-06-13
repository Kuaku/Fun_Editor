#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "../common.h"
#include "./tree.h"
#include "./render_queue.h"

#define NODE(sys, ...) \
    NodeCreateConfigured(&(sys)->tree_holder, (RenderNode){ \
        .parent = INVALID_NODE, \
        .first_child = INVALID_NODE, \
        .last_child = INVALID_NODE, \
        .previous_sibling = INVALID_NODE, \
        .next_sibling = INVALID_NODE, \
        __VA_ARGS__ \
    })

#define VNODE(sys, ...) NODE(sys, .layout = LAYOUT_VERTICAL,   __VA_ARGS__)
#define HNODE(sys, ...) NODE(sys, .layout = LAYOUT_HORIZONTAL, __VA_ARGS__)
#define LEAF(sys, ...)  NODE(sys, .layout = LAYOUT_NONE,       __VA_ARGS__)

typedef struct RenderWrapper RenderWrapper;

typedef int (*IntValueFunc)(RenderWrapper* wrapper);
typedef void (*DestroyCustomStateFunc)(RenderWrapper* wrapper);
typedef Position (*MeasureTextFunc)(RenderWrapper* wrapper, const char* text, float font_size, float spacing);
typedef void (*RenderQueueFunc)(RenderWrapper* wrapper, RenderQueue* queue);

struct RenderWrapper {
    IntValueFunc get_screen_height;
    IntValueFunc get_screen_width;
    DestroyCustomStateFunc destructor;
    MeasureTextFunc measure_text;
    RenderQueueFunc render_queue;

    void* custom_state;
};

typedef struct {
    NodeHandle handle;
    int z_index;
    size_t order;
} RenderEntry;

typedef struct RenderSystem {
    TreeHolder tree_holder;
    RenderWrapper render_wrapper;
    RenderQueue render_queue;
} RenderSystem;

RenderSystem InitRenderSystem(RenderWrapper wrapper);
void ClearRenderSystem(RenderSystem* system);
void CalculateSizingModel(Editor* editor, RenderSystem* system);
void GenerateRenderList(RenderSystem* system);
void BuildEditorRenderTree(Editor* editor);

#endif
