#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#include "../common.h"

typedef struct { unsigned char r, g, b, a; } RenderColor;

typedef struct {
    size_t offset;
    size_t length;
} TextRef;

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} StringArena;

typedef enum {
    RENDER_TEXT,
    RENDER_RECT,
    RENDER_CUT_PUSH,
    RENDER_CUT_POP
} RenderCommandType;

typedef struct {
    TextRef text;
    Position pos;
    float font_size;
    float spacing;
    RenderColor color;
} TextCommand;

typedef struct {
    Rect rect;
    RenderColor color;
} RectCommand;

typedef struct {
    Rect rect;
} ScissorPushCommand;

typedef struct {
    RenderCommandType type;
    union {
        TextCommand text;
        RectCommand rect;
        ScissorPushCommand scissor_push;
    } as;
} RenderCommand;

typedef struct {
    RenderCommand* commands;
    size_t command_capacity;
    size_t command_size;
    StringArena string_arena;
} RenderQueue;

StringArena InitStringArena(size_t initial_capacity);
void ClearStringArena(StringArena* arena);
void ResetStringArena(StringArena* arena);
TextRef ArenaPushString(StringArena* arena, const char* s, size_t len);
char* OffsetToPointer(StringArena* arena, TextRef ref);

RenderQueue InitRenderQueue(size_t initial_capacity);
void ClearRenderQueue(RenderQueue* queue);
void AppendRenderCommand(RenderQueue* queue, RenderCommand command);
void ResetRenderQueue(RenderQueue* queue);

void PushText(RenderQueue* queue, const char* s, Position pos, float font_size, float spacing, RenderColor color);
void PushRect(RenderQueue* queue, Rect rect, RenderColor color);
void PushScissor(RenderQueue* queue, Rect rect);
void PushScissorPop(RenderQueue* queue);

#endif
