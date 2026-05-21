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
        .background_color = (Color){32, 35, 41, 255},
        .mode_color = WHITE,
        .text_color = WHITE,
        .command_color = WHITE,
        .command_background_color = (Color){50, 54, 61, 255},
        .line_number_color = YELLOW
    };

    EditorSettings settings = {
        .scheme = scheme,
        .font_size = 25,
        .number_padding = 10,
        .pointer_padding = (Position){3, 3},
        .mode_padding = (Position){10, 10},
        .command_padding = (Position){10, 10},
        .pointer_width = 2,
    };
    
    int idx = 0;
    int count = 992 + 96; // ASCII/Latin + Geometric Shapes
    int* codepoints = malloc(count * sizeof(int));

    // ASCII + Latin (32-1023)
    for (int i = 32; i <= 1023; i++) codepoints[idx++] = i;
    // Geometric Shapes (0x25A0-0x25FF)
    for (int i = 0x25A0; i <= 0x25FF; i++) codepoints[idx++] = i;

    settings.editor_font = LoadFontEx("NotoSansJP-Regular.ttf", 60, codepoints, idx);
    free(codepoints);

    SetTextureFilter(settings.editor_font.texture, TEXTURE_FILTER_BILINEAR);

    char* path = NULL;
    if (argc >= 2) {
        path = strdup(argv[1]);
    }

    Editor editor = CreateEditor(settings, path);
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
