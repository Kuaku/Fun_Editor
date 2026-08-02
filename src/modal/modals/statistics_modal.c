#include "statistics_modal.h"
#include "../../editor/editor.h"
#include "../../statistics/statistics.h"

void StatisticsRender(Editor* editor, RenderNode* self) {
    Modal* modal = (Modal*)self->user_data;
    StatisticsState* state = (StatisticsState*)modal->state;
    StatisticSystem* stats = &editor->statistic_system;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int row_height = font_size + modal->style.widget_spacing;
    int pad = modal->style.content_padding.x;

    PushScissor(&editor->render_system.render_queue, self->inner_bounds);

    int y = self->inner_bounds.position.y + pad;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Frame Count: %llu",
             (unsigned long long)stats->frame_count);
    PushText(&editor->render_system.render_queue, buffer, (Position){self->inner_bounds.position.x + pad, y}, font_size, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});
    y += row_height;

    y += row_height / 2;
    PushLine(&editor->render_system.render_queue, (Position){self->inner_bounds.position.x + pad, y}, (Position){self->inner_bounds.position.x + self->inner_bounds.size.x - pad, y}, 1, (RenderColor){modal->style.border.r, modal->style.border.g, modal->style.border.b, modal->style.border.a});
    y += row_height / 2;

    const char* timer_names[] = {
        "Frame Timer",
        "Render Timer",
        "Update Timer",
        "File Polling Timer",
        "File Step Polling Timer",
        "Modal Update",
        "Input Timer"
    };

    for (int i = 0; i < EDITOR_TIMER_COUNT; i++) {
        if (i >= state->scroll_offset &&
            y < self->inner_bounds.position.y + self->inner_bounds.size.y - row_height) {

            StatisticTimer* timer = &stats->timers[i];

            PushText(&editor->render_system.render_queue, timer_names[i], (Position){self->inner_bounds.position.x + pad, y}, font_size, 1, (RenderColor){modal->style.focused_border.r, modal->style.focused_border.g, modal->style.focused_border.b, modal->style.focused_border.a});
            y += row_height;

            double avg_ms = TimerAverage(stats, i);
            snprintf(buffer, sizeof(buffer), "  Average: %.3f ms", avg_ms);
            PushText(&editor->render_system.render_queue, buffer, (Position){self->inner_bounds.position.x + pad * 2, y}, font_size * 0.8, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});
            y += row_height;

            double min_ms = timer->count > 0 ? timer->min_ns / 1000000.0 : 0.0;
            snprintf(buffer, sizeof(buffer), "  Min: %.3f ms", min_ms);
            PushText(&editor->render_system.render_queue, buffer, (Position){self->inner_bounds.position.x + pad * 2, y}, font_size * 0.8, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});
            y += row_height;

            double max_ms = timer->max_ns / 1000000.0;
            snprintf(buffer, sizeof(buffer), "  Max: %.3f ms", max_ms);
            PushText(&editor->render_system.render_queue, buffer, (Position){self->inner_bounds.position.x + pad * 2, y}, font_size * 0.8, 1, (RenderColor){modal->style.text.r, modal->style.text.g, modal->style.text.b, modal->style.text.a});
            y += row_height;

            snprintf(buffer, sizeof(buffer), "  Count: %llu",
                    (unsigned long long)timer->count);
            PushText(&editor->render_system.render_queue, buffer, (Position){self->inner_bounds.position.x + pad * 2, y}, font_size * 0.8, 1, (RenderColor){modal->style.text_muted.r, modal->style.text_muted.g, modal->style.text_muted.b, modal->style.text_muted.a});
            y += row_height + row_height / 2;
        }
    }

    PushScissorPop(&editor->render_system.render_queue);
}

void StatisticsInput(Modal* modal, RawInput input) {
    StatisticsState* state = (StatisticsState*)modal->state;

    switch (input.key) {
        case KEY_UP:
            if (state->scroll_offset > 0) {
                state->scroll_offset--;
            }
            break;
        case KEY_DOWN:
            if (state->scroll_offset < EDITOR_TIMER_COUNT - 1) {
                state->scroll_offset++;
            }
            break;
        case KEY_ESCAPE:
        case KEY_D:
            if (input.key == KEY_D && !HasModifiers(input.modifiers, MODI_CTRL)) {
                break;
            }
            CloseModal(&state->editor->modal_system, false);
            break;
    }
}

void StatisticsCleanup(void* raw_state) {
    StatisticsState* state = (StatisticsState*)raw_state;
    free(state);
}

void RegisterStatisticsModal(Editor* editor) {
    StatisticsState* state = calloc(1, sizeof(StatisticsState));
    state->editor = editor;
    state->scroll_offset = 0;

    Modal* modal = CreateModal(
        &editor->modal_system,
        "Performance Statistics",
        (Position){700, 600},
        StatisticsRender,
        NULL,
        StatisticsInput,
        StatisticsCleanup,
        NULL,
        state
    );

    modal->style.draw_title = true;
    modal->style.title_padding = (Position){10, 10};
    modal->is_cached = true;
    modal->margin = (Position){50, 50};

    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    RegisterModalToQuickCatch(&editor->modal_system, "statistics", modal);
}

void OpenStatisticsModal(Editor* editor) {
    PushModalFromCache(&editor->modal_system, "statistics");

    Modal* top = GetTopModal(&editor->modal_system);
    if (top) {
        StatisticsState* state = (StatisticsState*)top->state;
        state->scroll_offset = 0;
    }
}
