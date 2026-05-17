#ifndef STATISTICS_MODAL_H
#define STATISTICS_MODAL_H

#include "../modal.h"

typedef struct {
    Editor* editor;
    size_t scroll_offset;
} StatisticsState;

void StatisticsRender(Modal* modal, Rect content);
void StatisticsInput(Modal* modal, RawInput input);
void StatisticsCleanup(void* raw_state);

void RegisterStatisticsModal(Editor* editor);
void OpenStatisticsModal(Editor* editor);

#endif
