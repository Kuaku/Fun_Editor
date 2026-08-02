#ifndef STATISTICS_MODAL_H
#define STATISTICS_MODAL_H

#include "../modal.h"

typedef struct {
    Editor* editor;
    size_t scroll_offset;
} StatisticsState;

void StatisticsRender(Editor* editor, RenderNode* self);
void StatisticsInput(Modal* modal, RawInput input);
void StatisticsCleanup(void* raw_state);

void RegisterStatisticsModal(Editor* editor);
void OpenStatisticsModal(Editor* editor);

#endif
