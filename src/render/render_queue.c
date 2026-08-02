#include "render_queue.h"

StringArena InitStringArena(size_t initial_capacity) {
    StringArena out = {0};
    out.data = (char*)calloc(initial_capacity, sizeof(char));
    out.size = 0;
    out.capacity = initial_capacity;
    return out;
}

void ClearStringArena(StringArena* arena) {
    free(arena->data);
    arena->data = NULL;
    arena->size = 0;
    arena->capacity = 0;
}

void ResetStringArena(StringArena* arena) {
    arena->size = 0;
}

TextRef ArenaPushString(StringArena* arena, const char* s, size_t len) {
    while (arena->size + len + 1 > arena->capacity) {
        arena->capacity *= 2;
        arena->data = realloc(arena->data, arena->capacity * sizeof(char));
    }
    size_t offset = arena->size;
    memcpy(arena->data + offset, s, len);
    arena->data[offset + len] = '\0';
    arena->size += len + 1;
    return (TextRef){ .offset = offset, .length = len };
}

char* OffsetToPointer(StringArena* arena, TextRef ref) {
    return arena->data + ref.offset;
}

RenderQueue InitRenderQueue(size_t initial_capacity) {
    RenderQueue out = {0};
    out.commands = (RenderCommand*)calloc(initial_capacity, sizeof(RenderCommand));
    out.command_capacity = initial_capacity;
    out.string_arena = InitStringArena(INITIAL_STRING_ARENA_CAPACITY);
    return out;
}

void ClearRenderQueue(RenderQueue* queue) {
    ClearStringArena(&queue->string_arena);
    if (queue->commands) {
        free(queue->commands);
        queue->commands = NULL;
    }
}

void AppendRenderCommand(RenderQueue* queue, RenderCommand command) {
    while (queue->command_size >= queue->command_capacity) {
        queue->commands = realloc(queue->commands, queue->command_capacity * 2 * sizeof(RenderCommand));
        queue->command_capacity *= 2;
    }

    queue->commands[queue->command_size++] = command;
}

void ResetRenderQueue(RenderQueue* queue) {
    ResetStringArena(&queue->string_arena);
    queue->command_size = 0;
}

void PushText(RenderQueue* queue, const char* s, Position pos, float font_size, float spacing, RenderColor color) {
    TextRef string_ref = ArenaPushString(&queue->string_arena, s, strlen(s));
    RenderCommand command = {
        .type = RENDER_TEXT,
        .as.text.text = string_ref,
        .as.text.color = color,
        .as.text.pos = pos,
        .as.text.font_size = font_size,
        .as.text.spacing = spacing
    };
    AppendRenderCommand(queue, command);
}

void PushRect(RenderQueue* queue, Rect rect, RenderColor color) {
    RenderCommand command = {
        .type = RENDER_RECT,
        .as.rect.color = color,
        .as.rect.rect = rect,
    };
    AppendRenderCommand(queue, command);
}

void PushRectLines(RenderQueue* queue, Rect rect, RenderColor color, int thickness) {
    RenderCommand command = {
        .type = RENDER_RECT_LINES,
        .as.rect_lines.color = color,
        .as.rect_lines.rect = rect,
        .as.rect_lines.thickness = thickness
    };
    AppendRenderCommand(queue, command);
}

void PushLine(RenderQueue* queue, Position start, Position end, int thickness, RenderColor color) {
    RenderCommand command = {
        .type = RENDER_LINE,
        .as.line.start = start,
        .as.line.end = end,
        .as.line.thickness = thickness,
        .as.line.color = color
    };
    AppendRenderCommand(queue, command);
}

void PushScissor(RenderQueue* queue, Rect rect) {
    RenderCommand command = {
        .type = RENDER_CUT_PUSH,
        .as.scissor_push.rect = rect
    };
    AppendRenderCommand(queue, command);
}

void PushScissorPop(RenderQueue* queue) {
    RenderCommand command = {
        .type = RENDER_CUT_POP
    };
    AppendRenderCommand(queue, command);
}