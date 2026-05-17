#include "string_input.h"
#include "../../editor/editor.h"
#include "../../utils/utf8.h"

void StringInputRender(Modal* modal, Rect content) {
    StringInputState* state = (StringInputState*)modal->state;
    Editor* editor = state->editor;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int pad = modal->style.content_padding.x;

    BeginScissorMode(content.position.x, content.position.y,
                     content.size.x, content.size.y);

    int input_height = font_size + 12;
    int input_y = content.position.y + (content.size.y - input_height) / 2;
    int input_x = content.position.x + pad;
    int input_w = content.size.x - pad * 2;

    DrawRectangle(input_x, input_y, input_w, input_height,
                  modal->style.input_background);
    DrawRectangleLinesEx(
        (Rectangle){input_x, input_y, input_w, input_height},
        modal->style.border_width,
        modal->style.focused_border
    );

    char before_cursor[512];
    size_t cursor_byte = utf8_codepoint_to_offset(state->buffer, state->cursor);
    strncpy(before_cursor, state->buffer, cursor_byte);
    before_cursor[cursor_byte] = '\0';
    Vector2 before_size = MeasureTextEx(font, before_cursor, font_size, 1);

    int text_x = input_x + 6;
    int text_y = input_y + 6;

    DrawTextEx(font, state->buffer,
               (Vector2){text_x, text_y},
               font_size, 1, modal->style.text);

    DrawRectangle(text_x + (int)before_size.x, text_y,
                  2, font_size, modal->style.text);

    EndScissorMode();
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
        !HasModifiers(input.modifiers, MODI_CTRL | MODI_ALT | MODI_SUPER) &&
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
    free(raw_state);
}

void PushStringInputModal(Editor* editor, const char* title,
                          ModalResultCallback on_result, void* user_data) {
    StringInputState* state = calloc(1, sizeof(StringInputState));
    state->editor = editor;

    Modal* modal = CreateModal(
        &editor->modal_system,
        title,
        (Position){500, 120},
        StringInputRender,
        NULL,
        StringInputInput,
        StringInputCleanup,
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
