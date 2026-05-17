#include "string_input.h"
#include "../../editor/editor.h"

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
    strncpy(before_cursor, state->buffer, state->cursor);
    before_cursor[state->cursor] = '\0';
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
                    memmove(&state->buffer[state->cursor - 1],
                            &state->buffer[state->cursor],
                            state->length - state->cursor);
                    state->cursor--;
                    state->length--;
                    state->buffer[state->length] = '\0';
                }
                return;
            case KEY_DELETE:
                if (state->cursor < state->length) {
                    memmove(&state->buffer[state->cursor],
                            &state->buffer[state->cursor + 1],
                            state->length - state->cursor - 1);
                    state->length--;
                    state->buffer[state->length] = '\0';
                }
                return;
            case KEY_LEFT:
                if (state->cursor > 0) state->cursor--;
                return;
            case KEY_RIGHT:
                if (state->cursor < state->length) state->cursor++;
                return;
        }
        return;
    }

    if (input.key >= 32 && input.key < 127 &&
        !HasModifiers(input.modifiers, MODI_CTRL | MODI_ALT | MODI_SUPER) &&
        state->length < sizeof(state->buffer) - 1) {
        memmove(&state->buffer[state->cursor + 1],
                &state->buffer[state->cursor],
                state->length - state->cursor);
        state->buffer[state->cursor] = (char)input.key;
        state->cursor++;
        state->length++;
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
