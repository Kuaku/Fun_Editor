#include "buffer_list.h"
#include "../../editor/editor.h"

void BufferListRender(Editor* editor, RenderNode* self) {
    Modal* modal = (Modal*)self->user_data;
    Rect content = self->inner_bounds;
    BufferListState* state = (BufferListState*)modal->state;
    EditorState* es = &editor->state;
    int font_size = editor->settings.font_size;
    int row_height = font_size + modal->style.widget_spacing;
    size_t visible_rows = self->inner_bounds.size.y / row_height;

    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    if (state->selected_index >= state->scroll_offset + visible_rows) {
        state->scroll_offset = state->selected_index - visible_rows + 1;
    }

    PushScissor(&editor->render_system.render_queue, self->inner_bounds);

    for (size_t i = state->scroll_offset; i < es->text_buffers_count; i++) {
        size_t display_i = i - state->scroll_offset;
        if (display_i >= visible_rows) break;

        int y = self->inner_bounds.position.y + display_i * row_height;

        const char* full_path = es->text_buffers[i].file_path;
        const char* label = "[untitled]";

        if (full_path) {
            const char* slash = strrchr(full_path, '/');
            const char* bslash = strrchr(full_path, '\\');
            if (bslash && (!slash || bslash > slash)) slash = bslash;
            label = slash ? slash + 1 : full_path;
        }

        const char* prefix = (i == state->selected_index) ? "> " : "  ";
        RenderColor text_color = modal->style.text;
        if ((int)i == es->open_text_buffer_index) {
            text_color = modal->style.focused_border;
        }
        PushText(&editor->render_system.render_queue, TextFormat("%s%s", prefix, label), (Position){self->inner_bounds.position.x + 4, y + modal->style.widget_spacing / 2}, font_size, 1, text_color);
    }
    PushScissorPop(&editor->render_system.render_queue);
}

void BufferListInput(Modal* modal, RawInput input) {
    BufferListState* state = (BufferListState*)modal->state;
    size_t count = state->editor->state.text_buffers_count;

    switch (input.key) {
        case KEY_DOWN:
            if (state->selected_index + 1 < count) {
                state->selected_index++;
            }
            break;
        case KEY_UP:
            if (state->selected_index > 0) {
                state->selected_index--;
            }
            break;
        case KEY_ENTER:
            modal->result_data = (void*)(uintptr_t)state->selected_index;
            CloseModal(&state->editor->modal_system, true);
            break;
        case KEY_ESCAPE:
            CloseModal(&state->editor->modal_system, false);
            break;
    }
}

void BufferListResult(Modal* modal, bool confirmed, void* result, void* user_data) {
    if (!confirmed) return;

    Editor* editor = (Editor*)user_data;
    size_t index = (size_t)(uintptr_t)result;

    if (index < editor->state.text_buffers_count) {
        editor->state.open_text_buffer_index = (int)index;
    }
}

void BufferListCleanup(void* state) {
    free(state);
}

void RegisterBufferListModal(Editor* editor) {
    BufferListState* state = calloc(1, sizeof(BufferListState));
    state->editor = editor;
    state->selected_index = 0;
    state->scroll_offset = 0;

    Modal* modal = CreateModal(
        &editor->modal_system,
        "Open Buffers",
        (Position){500, 400},
        BufferListRender,
        NULL,
        BufferListInput,
        BufferListCleanup,
        NULL,
        state
    );
    modal->style.draw_title = true;
    modal->style.title_padding = (Position){10, 10};
    modal->on_result = BufferListResult;
    modal->on_result_user_data = editor;
    modal->is_cached = true;
    modal->margin = (Position){50, 50};

    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    RegisterModalToQuickCatch(&editor->modal_system, "buffer_list", modal);
}

void OpenBufferListModal(Editor* editor) {
    PushModalFromCache(&editor->modal_system, "buffer_list");

    Modal* top = GetTopModal(&editor->modal_system);
    if (top) {
        BufferListState* state = (BufferListState*)top->state;
        state->selected_index = editor->state.open_text_buffer_index >= 0
                              ? (size_t)editor->state.open_text_buffer_index : 0;
        state->scroll_offset = 0;
    }
}
