#ifndef RAYLIB_WRAPPER_H
#define RAYLIB_WRAPPER_H
#include "raylib.h"
#include "./render_system.h"

typedef struct {
    Font font;
    int load_size;
} RaylibWrapperState;

void DestroyRaylibRenderWrapper(RenderWrapper* wrapper);
RenderWrapper CreateRaylibRenderWrapper(char* font_path, int load_size);

Color RenderColorToColor(RenderColor in);
Vector2 PositionToVector2(Position in);

#endif