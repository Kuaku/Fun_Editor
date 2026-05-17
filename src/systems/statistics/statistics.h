#ifndef STATISTICS_H
#define STATISTICS_H

#include "common.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    FRAME_TIMER = 0,
    RENDER_TIMER,
    UPDATE_TIMER,
    FILE_POLLING_TIMER,
    FILE_POLLING_STEP_TIMER,
    MODAL_UPDATE,
    INPUT_TIMER,
    EDITOR_TIMER_COUNT
} EditorTimer;

typedef struct {
    uint64_t start_ns;
    uint64_t total_ns;
    uint64_t count;
    uint64_t min_ns;
    uint64_t max_ns;
} StatisticTimer;

typedef struct {
    StatisticTimer* timers;
    size_t count;
    size_t capacity;
    uint64_t frame_count;
} StatisticSystem;


void InitStatisticSystem(StatisticSystem* system);

void ClearStatisticsSystem(StatisticSystem* system);

void TimerStart(StatisticSystem* system, uint32_t index);

void TimerEnd(StatisticSystem* system, uint32_t index);

double TimerAverage(StatisticSystem* system, uint32_t index);

#endif