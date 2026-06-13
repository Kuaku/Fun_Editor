#include "./raylib_wrapper.h"

void DestroyRaylibRenderWrapper(RenderWrapper* wrapper) {
    if (!wrapper->custom_state) return;   
    RaylibWrapperState* state = (RaylibWrapperState*)wrapper->custom_state;
    UnloadFont(state->font);
    free(wrapper->custom_state);
    wrapper->custom_state = NULL;
}

static int RaylibGetScreenHeightWrapper(RenderWrapper* wrapper) {
    return GetScreenHeight();
}

static int RaylibGetScreenWidthWrapper(RenderWrapper* wrapper) {
    return GetScreenWidth();
}

static Position RaylibMeasureTextWrapper(RenderWrapper* wrapper, const char* text, float font_size, float spacing) {
    RaylibWrapperState* state = (RaylibWrapperState*)wrapper->custom_state;
    Vector2 result = MeasureTextEx(state->font, text, font_size, spacing);
    return (Position){result.x, result.y};
}

static void RaylibRenderQueue(RenderWrapper* wrapper, RenderQueue* queue) {
    RaylibWrapperState* state = (RaylibWrapperState*)wrapper->custom_state;
    for (size_t i = 0; i < queue->command_size; i++) {
        RenderCommand* command = &queue->commands[i];
        switch (command->type)
        {
        case RENDER_RECT:
            RectCommand* rect_command = (RectCommand*)&command->as;
            DrawRectangle(rect_command->rect.position.x, rect_command->rect.position.y, rect_command->rect.size.x, rect_command->rect.size.y, RenderColorToColor(rect_command->color));
            break;
        case RENDER_TEXT:
            TextCommand* text_command = (TextCommand*)&command->as;
            DrawTextEx(state->font, OffsetToPointer(&queue->string_arena, text_command->text), PositionToVector2(text_command->pos), text_command->font_size, text_command->spacing, RenderColorToColor(text_command->color));
            break;
        case RENDER_CUT_PUSH:
            // TODO: introduce real scissor stack
            ScissorPushCommand* scissor_command = (ScissorPushCommand*)&command->as;
            BeginScissorMode(scissor_command->rect.position.x, scissor_command->rect.position.y, scissor_command->rect.size.x, scissor_command->rect.size.y);
            break;
        case RENDER_CUT_POP:
            EndScissorMode();
            break;
        default:
            printf("RaylibRenderQueue: Type %d is not implemented for raylib renderer", command->type);
        }
    }
}

RenderWrapper CreateRaylibRenderWrapper(char* font_path, int load_size) {
    RaylibWrapperState* state = calloc(1, sizeof(RaylibWrapperState));
    int idx = 0;
    int count = 992 + 96; // ASCII/Latin + Geometric Shapes
    int* codepoints = malloc(count * sizeof(int));

    // ASCII + Latin (32-1023)
    for (int i = 32; i <= 1023; i++) codepoints[idx++] = i;
    // Geometric Shapes (0x25A0-0x25FF)
    for (int i = 0x25A0; i <= 0x25FF; i++) codepoints[idx++] = i;

    state->font = LoadFontEx(font_path, load_size, codepoints, idx);
    free(codepoints);

    return (RenderWrapper) {
        .get_screen_height  = RaylibGetScreenHeightWrapper,
        .get_screen_width   = RaylibGetScreenWidthWrapper,
        .destructor         = DestroyRaylibRenderWrapper,
        .measure_text       = RaylibMeasureTextWrapper,
        .render_queue       = RaylibRenderQueue,
        .custom_state       = state
    };
}

Color RenderColorToColor(RenderColor in) {
    return (Color){in.r, in.g, in.b, in.a};
}

Vector2 PositionToVector2(Position in) {
    return (Vector2){in.x, in.y};
}