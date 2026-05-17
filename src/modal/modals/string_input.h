#ifndef STRING_INPUT_H
#define STRING_INPUT_H

#include "../modal.h"

typedef struct {
    Editor* editor;
    char buffer[512];
    size_t cursor;
    size_t length;
} StringInputState;

void StringInputRender(Modal* modal, Rect content);
void StringInputInput(Modal* modal, RawInput input);
void StringInputCleanup(void* raw_state);

void PushStringInputModal(Editor* editor, const char* title, ModalResultCallback on_result, void* user_data);

#endif
