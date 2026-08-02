#include "string_input.h"
#include "../../editor/editor.h"
#include "../../utils/utf8.h"

void StringInputRender(Editor* editor, RenderNode* self) {
    Modal* modal = (Modal*)self->user_data;
    StringInputState* state = (StringInputState*)modal->state;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int pad = modal->style.content_padding.x;

    PushScissor(&editor->render_system.render_queue, self->inner_bounds);

    int input_height = font_size + 12;
    int input_y = self->inner_bounds.position.y + (self->inner_bounds.size.y - input_height) / 2;
    int input_x = self->inner_bounds.position.x + pad;
    int input_w = self->inner_bounds.size.x - pad * 2;

    PushRect(&editor->render_system.render_queue, (Rect){{input_x, input_y}, {input_w, input_height}}, (RenderColor){modal->style.input_background.r, modal->style.input_background.g, modal->style.input_background.b, modal->style.input_background.a});
    PushRectLines(&editor->render_system.render_queue, (Rect){{input_x, input_y}, {input_w, input_height}}, (RenderColor){modal->style.focused_border.r, modal->style.focused_border.g, modal->style.focused_border.b, modal->style.focused_border.a}, modal->style.border_width);

    int text_x = input_x + 6;
    int text_y = input_y + 6;

    if (state->prefix) {
        Position prefix_size = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, state->prefix, font_size, 1);
        PushText(&editor->render_system.render_queue, state->prefix, (Position){text_x, text_y}, font_size, 1, (RenderColor){modal->style.text_muted.r, modal->style.text_muted.g, modal->style.text_muted.b, modal->style.text_muted.a});
        text_x += (int)prefix_size.x;
    }

    char before_cursor[512];
    size_t cursor_byte = utf8_codepoint_to_offset(state->buffer, state->cursor);
    strncpy(before_cursor, state->buffer, cursor_byte);
    before_cursor[cursor_byte] = '\0';
    Position before_size = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, before_cursor, font_size, 1);

    PushText(&editor->render_system.render_queue, state->buffer, (Position){text_x, text_y}, font_size, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});
    PushRect(&editor->render_system.render_queue, (Rect){{text_x + (int)before_size.x, text_y}, {2, font_size}}, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});

    PushScissorPop(&editor->render_system.render_queue);
}

void StringInputInput(Modal* modal, RawInput input) {
    StringInputState* state = (StringInputState*)modal->state;
    Editor* editor = state->editor;

    if (!input.is_char) {
        switch (input.key) {
            case KEY_ENTER: {
                char* result = malloc(state->length + 1);
                memcpy(result, state->buffer, state->length);
                result[state->length] = '\0';
                modal->result_data = result;
                CloseModal(&editor->modal_system, true);
                return;
            }
            case KEY_ESCAPE:
                CloseModal(&editor->modal_system, false);
                return;
            case KEY_BACKSPACE:
                if (state->cursor > 0) {
                    size_t cursor_byte = utf8_codepoint_to_offset(state->buffer, state->cursor);
                    size_t utf8_length = utf8_prev(state->buffer, state->buffer + cursor_byte);
                    memmove(&state->buffer[cursor_byte - utf8_length],
                            &state->buffer[cursor_byte],
                            state->length - cursor_byte);
                    state->cursor--;
                    state->length -= utf8_length;
                    state->codepoint_count--;
                    state->buffer[state->length] = '\0';
                }
                return;
            case KEY_DELETE:
                if (state->cursor < state->codepoint_count) {
                    size_t cursor_byte = utf8_codepoint_to_offset(state->buffer, state->cursor);
                    uint32_t codepoint;
                    size_t utf8_length = utf8_decode(state->buffer + cursor_byte, &codepoint);
                    
                    memmove(&state->buffer[cursor_byte],
                            &state->buffer[cursor_byte + utf8_length],
                            state->length - cursor_byte - utf8_length);
                    state->length -= utf8_length;
                    state->codepoint_count--;
                    state->buffer[state->length] = '\0';
                }
                return;
            case KEY_LEFT:
                if (state->cursor > 0) state->cursor--;
                return;
            case KEY_RIGHT:
                if (state->cursor < state->codepoint_count) state->cursor++;
                return;
        }
        return;
    }

    if (input.key >= 32 &&
        state->length < sizeof(state->buffer) - 1) {
        char utf8_text_buffer[4];
        size_t utf8_length = utf8_encode(input.key, utf8_text_buffer);
        // TODO: Catch length = 0
        size_t cursor_byte = utf8_codepoint_to_offset(state->buffer, state->cursor);
        
        memmove(&state->buffer[cursor_byte + utf8_length],
                &state->buffer[cursor_byte],
                state->length - cursor_byte);
        memmove(&state->buffer[cursor_byte],
                utf8_text_buffer,
                utf8_length);
        state->cursor++;
        state->length += utf8_length;
        state->codepoint_count++;
        state->buffer[state->length] = '\0';
    }
}

void StringInputCleanup(void* raw_state) {
    StringInputState* state = (StringInputState*)raw_state;
    if (state->prefix != NULL) {
        free(state->prefix);
    }
    free(raw_state);
}

void PushStringInputModal(Editor* editor, const char* title,
                          ModalResultCallback on_result, void* user_data, const char* prefix) {
    StringInputState* state = calloc(1, sizeof(StringInputState));
    state->prefix = prefix ? strdup(prefix) : NULL;

    state->editor = editor;

    Modal* modal = CreateModal(
        &editor->modal_system,
        title,
        (Position){500, 120},
        StringInputRender,
        NULL,
        StringInputInput,
        StringInputCleanup,
        NULL,
        state
    );
    modal->style.draw_title = true;
    modal->style.title_padding = (Position){10, 10};
    modal->on_result = on_result;
    modal->on_result_user_data = user_data;
    modal->margin = (Position){50, 50};

    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    PushModal(&editor->modal_system, modal);
}
