#ifndef BUFFER_LIST_H
#define BUFFER_LIST_H

#include "../modal.h"

typedef struct {
    Editor* editor;
    size_t selected_index;
    size_t scroll_offset;
} BufferListState;

void BufferListRender(Modal* modal, Rect content);
void BufferListInput(Modal* modal, RawInput input);
void BufferListResult(Modal* modal, bool confirmed, void* result, void* user_data);
void BufferListCleanup(void* state);

void RegisterBufferListModal(Editor* editor);
void OpenBufferListModal(Editor* editor);

#endif
