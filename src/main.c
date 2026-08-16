#include "editor/editor.h"
#include "render/render.h"
#include "./modal/modals/buffer_list.h"
#include "./modal/modals/file_explorer.h"
#include "./modal/modals/statistics_modal.h"

Vector2 PositionToVector(Position position) {
    return (Vector2){position.x, position.y};
}

void SetupWindow() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 700, "Fun Editor");
    MaximizeWindow();

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
}

int main(int argc, char** argv) {
    SetupWindow();

    ColorScheme scheme = {
        .background_color = (RenderColor){32, 35, 41, 255},
        .mode_color = (RenderColor){255, 255, 255, 255},
        .text_color = (RenderColor){255, 255, 255, 255},
        .command_color = (RenderColor){255, 255, 255, 255},
        .command_background_color = (RenderColor){50, 54, 61, 255},
        .line_number_color = (RenderColor){ 253, 249, 0, 255 },
        .selection_background_color = (RenderColor){255, 255, 255, 255},
        .selection_foreground_color = (RenderColor){32, 35, 41, 255}
    };

    EditorSettings settings = {
        .scheme = scheme,
        .font_size = 30,
        .number_padding = 10,
        .pointer_padding = (Position){3, 3},
        .mode_padding = (Position){10, 10},
        .command_padding = (Position){10, 10},
        .pointer_width = 2,
        .key_repeat_delay = 400,
        .key_repeat_interval = 30,
        .font_loading_size = 30,
        .font_path = "NotoSansJP-Regular.ttf"
    };
    
    char* path = NULL;
    if (argc >= 2) {
        path = strdup(argv[1]);
    }
    Editor editor;
    CreateEditor(&editor, settings, path);
    RegisterBufferListModal(&editor);
    RegisterFileExplorerModal(&editor);
    RegisterStatisticsModal(&editor);
    RegisterDefaultCommandBinding(&editor.input_system.command_system);

    if (path) {
        free(path);
        path = NULL;
    }

    while (!WindowShouldClose() && !ShouldEditorClose(&editor)) {
        TimerStart(&editor.statistic_system, FRAME_TIMER);
        editor.statistic_system.frame_count++;

        TimerStart(&editor.statistic_system, INPUT_TIMER);
        EditorHandleInput(&editor);
        TimerEnd(&editor.statistic_system, INPUT_TIMER);

        TimerStart(&editor.statistic_system, UPDATE_TIMER);
        EditorHandleUpdate(&editor);
        TimerEnd(&editor.statistic_system, UPDATE_TIMER);

        BeginDrawing();

        TimerStart(&editor.statistic_system, RENDER_TIMER);
        EditorRender(&editor);
        TimerEnd(&editor.statistic_system, RENDER_TIMER);

        EndDrawing();
        TimerEnd(&editor.statistic_system, FRAME_TIMER);
    }

    ClearEditor(&editor);
    return 0;
}
