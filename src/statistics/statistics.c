#include "statistics.h"
#include "../platform/platform.h"

StatisticSystem InitStatisticSystem() {
    StatisticSystem system = {0};

    system.capacity = EDITOR_TIMER_COUNT;
    system.timers = calloc(system.capacity, sizeof(StatisticTimer));

    return system;
}

void ClearStatisticsSystem(StatisticSystem* system) {
    free(system->timers);
    system->timers = NULL;
    system->count = 0;
    system->capacity = 0;
    system->frame_count = 0;
}

void TimerStart(StatisticSystem* system, uint32_t index) {
    system->timers[index].start_ns = GetTimeNs();
}

void TimerEnd(StatisticSystem* system, uint32_t index) {
    StatisticTimer* timer = &system->timers[index];
    uint64_t elapsed = GetTimeNs() - timer->start_ns;
    timer->total_ns += elapsed;
    timer->count++;

    if (timer->count == 1 || elapsed < timer->min_ns) {
        timer->min_ns = elapsed;
    }
    if (elapsed > timer->max_ns) {
        timer->max_ns = elapsed;
    }
}

double TimerAverage(StatisticSystem* system, uint32_t index) {
    StatisticTimer* timer = &system->timers[index];
    if (timer->count == 0) return 0.0;
    return (timer->total_ns / timer->count) / 1000000.0;
}
