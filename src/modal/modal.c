#include "modal.h"
#include "../editor/editor.h"

void ApplyWantedSize(Modal* modal) {
    modal->bounds.size = modal->wanted_size;
}

void ApplyMaxSize(Modal* modal) {
    if (modal->max_size.x > 0 && modal->bounds.size.x > modal->max_size.x) {
        modal->bounds.size.x = modal->max_size.x;
    }
    if (modal->max_size.y > 0 && modal->bounds.size.y > modal->max_size.y) {
        modal->bounds.size.y = modal->max_size.y;
    }
}

void ApplyMinSize(Modal* modal) {
    if (modal->min_size.x > 0 && modal->bounds.size.x < modal->min_size.x) {
        modal->bounds.size.x = modal->min_size.x;
    }
    if (modal->min_size.y > 0 && modal->bounds.size.y < modal->min_size.y) {
        modal->bounds.size.y = modal->min_size.y;
    }
}

void ApplyMargin(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    size_t max_w = sw - modal->margin.x * 2;
    size_t max_h = sh - modal->margin.y * 2;
    if (modal->bounds.size.x > max_w) {
        modal->bounds.size.x = max_w;
    }
    if (modal->bounds.size.y > max_h) {
        modal->bounds.size.y = max_h;
    }
}

void CenterModal(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    modal->bounds.position.x = (sw - modal->bounds.size.x) / 2;
    modal->bounds.position.y = (sh - modal->bounds.size.y) / 2;
}

void ModalAddLayout(Modal* modal, LayoutFunc layout) {
    if (modal->layouts == NULL) {
        modal->layout_capacity = 4;
        modal->layouts = calloc(modal->layout_capacity, sizeof(LayoutFunc));
    }
    if (modal->layout_count >= modal->layout_capacity) {
        modal->layout_capacity *= 2;
        modal->layouts = realloc(modal->layouts, modal->layout_capacity * sizeof(LayoutFunc));
    }
    modal->layouts[modal->layout_count++] = layout;
}

ModalSystem InitModalSystem() {
    ModalSystem system;
    system.stack_capacity = INITIAL_MODAL_BUFFER_CAPACITY;
    system.stack = calloc(system.stack_capacity, sizeof(Modal*));
    system.stack_count = 0;

    system.cache_capacity = INITIAL_MODAL_CACHE_BUFFER_CAPACITY;
    system.modal_cache = calloc(system.cache_capacity, sizeof(Modal*));
    system.modal_keys = calloc(system.cache_capacity, sizeof(char*));
    system.cache_count = 0;

    system.default_style = (ModalStyle){
        .background    = (RenderColor){45, 48, 55, 240},
        .border        = (RenderColor){80, 85, 95, 255},
        .text          = (RenderColor){255, 255, 255, 255},
        .text_muted    = (RenderColor){150, 150, 150, 255},
        .selection     = (RenderColor){60, 100, 180, 255},
        .input_background = (RenderColor){30, 32, 38, 255},
        .focused_border   = (RenderColor){100, 140, 220, 255},
        .border_width     = 2,
        .content_padding  = (Position){12, 12},
        .widget_spacing   = 4,
        .title_height     = 32,
    };

    return system;
}

void RegisterModalToQuickCatch(ModalSystem* system, const char* key, Modal* modal) {
    if (system->cache_count >= system->cache_capacity) {
        system->cache_capacity *= 2;
        system->modal_cache = realloc(system->modal_cache, system->cache_capacity * sizeof(Modal*));
        system->modal_keys = realloc(system->modal_keys, system->cache_capacity * sizeof(char*));
    }
    size_t index = system->cache_count++;
    system->modal_cache[index] = modal;
    system->modal_keys[index] = strdup(key);
}

Modal* CreateModal(ModalSystem* system, const char* title, Position wanted_size, RenderFunc render, ModalUpdateFunc update, ModalInputFunc input, ModalCleanupFunc cleanup, ModalRepeatableFunc is_repeatable, void* state) {
    Modal* modal = calloc(1, sizeof(Modal));
    modal->title = strdup(title);
    modal->wanted_size = wanted_size;
    modal->style = system->default_style;
    modal->custom_render = render;
    modal->custom_input = input;
    modal->custom_update = update;
    modal->cleanup = cleanup;
    modal->state = state;
    modal->is_repeatable = is_repeatable;

    return modal;
}

void PushModal(ModalSystem* system, Modal* modal) {
    if (system->stack_count >= system->stack_capacity) {
        system->stack_capacity *= 2;
        system->stack = realloc(system->stack, system->stack_capacity * sizeof(Modal*));
    }
    system->stack[system->stack_count++] = modal;
}

void PushModalFromCache(ModalSystem* system, const char* key) {
    for (int i = 0; i < system->cache_count; i++) {
        if (strcmp(key, system->modal_keys[i]) == 0) {
          PushModal(system, system->modal_cache[i]);
          return;
        }
    }
}

Modal* GetTopModal(ModalSystem* system) {
    if (system->stack_count == 0) return NULL;
    return system->stack[system->stack_count - 1];
}

void ClearModal(Modal* modal) {
    if (modal->title) {
        free(modal->title);
    }

    if (modal->layouts) {
        free(modal->layouts);
    }

    if (modal->cleanup) {
        modal->cleanup(modal->state);
    }
}

void CloseModal(ModalSystem* system, bool confirmed) {
    if (system->stack_count == 0) return;

    Modal* modal = system->stack[--system->stack_count];

    if (modal->on_result) {
        modal->on_result(modal, confirmed, modal->result_data, modal->on_result_user_data);
    }

    if (modal->is_cached) return;

    ClearModal(modal);
    free(modal);
}

bool ModalSystemHasActive(ModalSystem* system) {
    return system->stack_count > 0;
}

void ClearModalSystem(ModalSystem* system) {
    system->stack_count = 0;

    for (size_t i = 0; i < system->cache_count; i++) {
        Modal* modal = system->modal_cache[i];
        ClearModal(modal);
        free(modal);
        free(system->modal_keys[i]);
    }

    free(system->stack);
    free(system->modal_cache);
    free(system->modal_keys);
}

void ModalResolveBounds(Modal* modal) {
    for (size_t i = 0; i < modal->layout_count; i++) {
        modal->layouts[i](modal);
    }
}

void ModalRenderBackdrop(Editor* editor, RenderNode* self) {
    PushRect(&editor->render_system.render_queue, self->inner_bounds, (RenderColor){0, 0, 0, 120});
}

void ModalGeneralRender(Editor* editor, RenderNode* self) {
    Modal* active_modal = (Modal*)self->user_data;
    PushScissor(&editor->render_system.render_queue, self->inner_bounds);
    PushRect(&editor->render_system.render_queue, self->inner_bounds, active_modal->style.background);
    
    if (active_modal->style.draw_title) {
        int title_height = active_modal->style.title_height + active_modal->style.title_padding.y * 2;

        PushText(&editor->render_system.render_queue, active_modal->title, (Position){self->inner_bounds.position.x + active_modal->style.title_padding.x, self->inner_bounds.position.y + active_modal->style.title_padding.y}, active_modal->style.title_height, 1, active_modal->style.text);
        self->inner_bounds.position.y += title_height;
        self->inner_bounds.size.y -= title_height;
    }

    if (active_modal->custom_render) {
        active_modal->custom_render(editor, self);
    }
    PushScissorPop(&editor->render_system.render_queue);
}

void ModalSystemAddNodes(Editor* editor) {
    ModalSystem* system = &editor->modal_system;
    if (system->stack_count == 0) return;

    NodeHandle backdrop = VNODE(&editor->render_system, .horizontal_axis_type = AXIS_FIXED, .vertical_axis_type = AXIS_FIXED, .position_type = POSITION_FIXED, .fixed_size = (Position){editor->render_system.render_wrapper.get_screen_width(&editor->render_system.render_wrapper), editor->render_system.render_wrapper.get_screen_height(&editor->render_system.render_wrapper)}, .fixed_position = {0, 0}, .custom_render = ModalRenderBackdrop, .z_index = 5);

    Modal* active_modal = GetTopModal(system);
    ModalResolveBounds(active_modal);
    NodeHandle active_modal_node = NODE(&editor->render_system, .horizontal_axis_type = AXIS_FIXED, .vertical_axis_type = AXIS_FIXED, .position_type = POSITION_FIXED, .fixed_size = active_modal->bounds.size, .fixed_position = active_modal->bounds.position, .custom_render = ModalGeneralRender, .user_data = active_modal);

    AppendChild(&editor->render_system.tree_holder, backdrop, active_modal_node);
}
