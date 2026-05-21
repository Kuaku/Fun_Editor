#include "render.h"

void ClearEditorSettings(EditorSettings* settings) {
    if (!settings) return;

    if (settings->editor_font.texture.id > 0) {
        UnloadFont(settings->editor_font);
    }
}

size_t GetPointerOffsetFromLeft(Editor* editor, TextBuffer* buffer, Position pointer) {
    char* temp = NULL;
    char* line = GenerateLine(buffer, pointer.y);
    size_t line_length = strlen(line);
    temp = calloc(line_length+1, sizeof(char));

    strncpy(temp, line, pointer.x);
    Vector2 draw_length = MeasureTextEx(editor->settings.editor_font, temp, editor->settings.font_size, 1);

    free(temp);
    free(line);
    return draw_length.x;
}

void RenderLineBufferWithSelection(Editor* editor, TextBuffer* buffer, char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position) {
    Vector2 draw_length = MeasureTextEx(editor->settings.editor_font, text_buffer, editor->settings.font_size, 1);


    if (position.y < selection_start_position.y || position.y > selection_end_position.y) {
        DrawTextEx(editor->settings.editor_font, text_buffer, drawPosition, editor->settings.font_size, 1, editor->settings.scheme.text_color);
        return;
    }

    char* before_buffer = NULL;
    char* line_buffer = NULL;
    char* after_buffer = NULL;
    Vector2 line_buffer_start = drawPosition;
    int buffer_start = 0;
    int buffer_end = line_length;
    if (selection_start_position.y == position.y && selection_start_position.x > position.x) {
        buffer_start = selection_start_position.x - position.x;
    }

    if (selection_end_position.y == position.y && selection_end_position.x < position.x + line_length) {
        buffer_end = selection_end_position.x - position.x;
    }

    if (buffer_start > 0) {
        before_buffer = calloc(buffer_start + 1, sizeof(char));
        strncpy(before_buffer, text_buffer, buffer_start);
        Vector2 before_buffer_size = MeasureTextEx(editor->settings.editor_font, before_buffer, editor->settings.font_size, 1);
        line_buffer_start.x += before_buffer_size.x;
    }

    int selected_length = buffer_end - buffer_start;
    if (selected_length > 0) {
        line_buffer = calloc(selected_length + 1, sizeof(char));
        strncpy(line_buffer, text_buffer + buffer_start, selected_length);
    }

     if (buffer_end < line_length) {
        int after_length = line_length - buffer_end;
        after_buffer = calloc(after_length + 1, sizeof(char));
        strncpy(after_buffer, text_buffer + buffer_end, after_length);
    }

    if (before_buffer != NULL) {
        DrawTextEx(editor->settings.editor_font, before_buffer, drawPosition, editor->settings.font_size, 1, editor->settings.scheme.text_color);
    }

    if (line_buffer != NULL) {
        Vector2 line_buffer_size = MeasureTextEx(editor->settings.editor_font, line_buffer, editor->settings.font_size, 1);
        DrawRectangle(line_buffer_start.x, line_buffer_start.y, line_buffer_size.x, line_buffer_size.y, editor->settings.scheme.text_color);
        DrawTextEx(editor->settings.editor_font, line_buffer, line_buffer_start, editor->settings.font_size, 1, editor->settings.scheme.background_color);
    }

    if (after_buffer != NULL) {
        Vector2 line_buffer_size = MeasureTextEx(editor->settings.editor_font, line_buffer != NULL ? line_buffer : "", editor->settings.font_size, 1);
        Vector2 after_start = line_buffer_start;
        after_start.x += line_buffer_size.x;
        DrawTextEx(editor->settings.editor_font, after_buffer, after_start, editor->settings.font_size, 1, editor->settings.scheme.text_color);
    }

    free(before_buffer);
    free(line_buffer);
    free(after_buffer);
}

void RenderLineBuffer(Editor* editor, TextBuffer* buffer, char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position) {
    if (buffer->has_selection && position.y >= selection_start_position.y && position.y <= selection_end_position.y) {
        RenderLineBufferWithSelection(editor, buffer, text_buffer, position, line_length, drawPosition, selection_start_position, selection_end_position);
    } else {
        DrawTextEx(editor->settings.editor_font, text_buffer, drawPosition, editor->settings.font_size, 1, editor->settings.scheme.text_color);
    }
}

void RenderLine(Editor* editor, TextBuffer* buffer, int y_line, Position position, size_t index, Position pointer, Position selection_start_position, Position selection_end_position) {
    char* temp = NULL;
    size_t line_buffer_length;
    char* line = GenerateLine(buffer, index);
    size_t line_length = strlen(line);
    if (pointer.y != index) {
        RenderLineBuffer(editor, buffer, line, (Position){0, y_line}, line_length, PositionToVector(position), selection_start_position, selection_end_position);
    } else {
        if (temp != NULL) {
            free(temp);
        }
        temp = calloc(line_length + 1, sizeof(char));
        strncpy(temp, line, pointer.x);
        Vector2 draw_length = MeasureTextEx(editor->settings.editor_font, temp, editor->settings.font_size, 1);
        RenderLineBuffer(editor, buffer, temp, (Position){0, y_line}, pointer.x, PositionToVector(position), selection_start_position, selection_end_position);
        RenderLineBuffer(editor, buffer, line + pointer.x, (Position){pointer.x, y_line}, line_length - pointer.x, (Vector2){position.x + draw_length.x + editor->settings.pointer_padding.x * 2 + editor->settings.pointer_width, position.y}, selection_start_position, selection_end_position);
        DrawRectangle(position.x + draw_length.x + editor->settings.pointer_padding.x, position.y, editor->settings.pointer_width, editor->settings.font_size - 2 * editor->settings.pointer_padding.y, editor->settings.scheme.text_color);
    }
    free(line);
    if (temp != NULL) {
        free(temp);
    }
}

void EditorRenderBar(Editor* editor) {
    int screen_height = GetScreenHeight();
    int screen_width = GetScreenWidth();
    DrawRectangle(0, screen_height - editor->settings.font_size - 2 * editor->settings.command_padding.y, screen_width,editor->settings.font_size + 2 * editor->settings.command_padding.y, editor->settings.scheme.command_background_color);
    if (editor->input_system.current_mode == MODE_COMMAND) {
        EditorRenderCommand(editor);
    } else {
        EditorRenderStatusBar(editor);    
    }
}

void EditorRenderStatusBar(Editor* editor) {
    int screen_height = GetScreenHeight();
    int screen_width = GetScreenWidth();
    Position offset = (Position){editor->settings.command_padding.x, screen_height - editor->settings.command_padding.y - editor->settings.font_size};
        
    

    TextBuffer* buffer = GetActiveBuffer(editor);
    Position cursor = GetPointerCodePosition(buffer);

    int length = snprintf(NULL, 0, "%d | %d", cursor.x + 1, cursor.y + 1);
    char* position_str = (char*)malloc(length + 1); 
    snprintf(position_str, length + 1, "%d | %d", cursor.x + 1, cursor.y + 1);

    Vector2 bounding_position_str = MeasureTextEx(editor->settings.editor_font, position_str, editor->settings.font_size, 1);
    DrawTextEx(editor->settings.editor_font, position_str, (Vector2){screen_width - editor->settings.command_padding.x - bounding_position_str.x, screen_height - editor->settings.command_padding.y - bounding_position_str.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);

    free(position_str);

    int right_bounding_width = bounding_position_str.x + 2 * editor->settings.command_padding.x;

    DrawRectangle(screen_width - right_bounding_width - 2, screen_height - editor->settings.command_padding.y - editor->settings.font_size, 2, editor->settings.font_size, editor->settings.scheme.command_color);

    right_bounding_width += 2;

    BeginScissorMode(editor->settings.command_padding.x, screen_height - editor->settings.font_size - editor->settings.command_padding.y, screen_width - editor->settings.command_padding.x - right_bounding_width, editor->settings.font_size);

    const char* full_path = buffer->file_path;
    const char* label = "[untitled]";
    if (full_path) {
        label = full_path;
    }

    Vector2 bounding_file_path = MeasureTextEx(editor->settings.editor_font, label, editor->settings.font_size, 1);
    DrawTextEx(editor->settings.editor_font, label, (Vector2){editor->settings.command_padding.x, screen_height - editor->settings.font_size - editor->settings.command_padding.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);

    EndScissorMode();
}

void EditorRenderCommand(Editor* editor) {
    int screen_height = GetScreenHeight();
    Position offset = (Position){editor->settings.command_padding.x, screen_height - editor->settings.command_padding.y - editor->settings.font_size};
    DrawTextEx(editor->settings.editor_font, ":", (Vector2){offset.x, offset.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);
    Vector2 offset_prefix = MeasureTextEx(editor->settings.editor_font, ": ", editor->settings.font_size, 1);
    
    char* temp;
    temp = calloc(editor->input_system.command_system.pointer_position + 1, sizeof(char));
    strncpy(temp, editor->input_system.command_system.command_buffer, editor->input_system.command_system.pointer_position);
    DrawTextEx(editor->settings.editor_font, temp, (Vector2){offset.x + offset_prefix.x, offset.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);
    Vector2 offset_first_part = MeasureTextEx(editor->settings.editor_font, temp, editor->settings.font_size, 1);
    DrawRectangle(offset.x + offset_prefix.x + offset_first_part.x + editor->settings.pointer_padding.x, offset.y + editor->settings.pointer_padding.y, editor->settings.pointer_width, editor->settings.font_size - editor->settings.pointer_padding.y * 2, WHITE);
    char* last_part = editor->input_system.command_system.command_buffer + editor->input_system.command_system.pointer_position;
    DrawTextEx(editor->settings.editor_font, last_part, (Vector2){offset.x + offset_prefix.x + offset_first_part.x + editor->settings.pointer_padding.x * 2 + editor->settings.pointer_width, offset.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);
    free(temp);
}

void EditorRenderTextBuffer(Editor* editor, Rect render_field) {
    TextBuffer* buffer = &editor->state.text_buffers[editor->state.open_text_buffer_index];
    Position pointer = GetPointerPosition(buffer);
    Position selection_start_position = {0};
    Position selection_end_position = {0};

    if (buffer->has_selection) {
        selection_start_position = IndexToPosition(buffer, min(buffer->selection_start, buffer->selection_end));
        selection_end_position = IndexToPosition(buffer, max(buffer->selection_start, buffer->selection_end));
    }

    size_t pointer_offset = GetPointerOffsetFromLeft(editor, buffer, pointer);
    size_t lines_completly_rendered = render_field.size.y / editor->settings.font_size;
    size_t line_number = buffer->line_anchor;
    size_t line_count = GetLineCount(buffer);
    BeginScissorMode(BREAK_DOWN_RECT(render_field));
    if (pointer.y >= buffer->line_anchor + lines_completly_rendered) {
        buffer->line_anchor = pointer.y - lines_completly_rendered + 1;
    }
    if (pointer.y <= buffer->line_anchor) {
        buffer->line_anchor = pointer.y;
    }

    if (buffer->offset_x + render_field.size.x <= pointer_offset) {
        buffer->offset_x = pointer_offset - render_field.size.x + editor->settings.pointer_padding.x * 2 + editor->settings.pointer_width;
    }
    if (buffer->offset_x > pointer_offset) {
        buffer->offset_x = pointer_offset;
    }

    size_t line_y = 0;
    for (size_t i = buffer->line_anchor; i < min(buffer->line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        RenderLine(editor, buffer, i, (Position){render_field.position.x-buffer->offset_x, render_field.position.y + line_y * editor->settings.font_size}, i, pointer, selection_start_position, selection_end_position);
        line_y++;
    }
    EndScissorMode();
}

void EditorRenderTextField(Editor* editor, Rect render_field) {
    TextBuffer* buffer = &editor->state.text_buffers[editor->state.open_text_buffer_index];
    Position pointer = GetPointerPosition(buffer);
    size_t pointer_offset = GetPointerOffsetFromLeft(editor, buffer, pointer);
    size_t lines_completly_rendered = render_field.size.y / editor->settings.font_size;
    size_t line_number = buffer->line_anchor;
    size_t line_count = GetLineCount(buffer);

    size_t max_offset = 0;
    size_t digits = snprintf(NULL, 0, "%zu", min(buffer->line_anchor + lines_completly_rendered + 1, line_count) + 1);
    size_t local_offset = 0;
    Vector2 measured_text;
    char* number_str  = malloc(digits + 1);
    for (size_t i = buffer->line_anchor; i < min(buffer->line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = MeasureTextEx(editor->settings.editor_font, number_str, editor->settings.font_size, 1);
        local_offset = measured_text.x;
        if (local_offset > max_offset) {
            max_offset = local_offset;
        }
    }
    max_offset += editor->settings.number_padding * 2;
    Rect text_buffer_field = (Rect){render_field.position.x + max_offset, render_field.position.y, render_field.size.x - max_offset, render_field.size.y};
    EditorRenderTextBuffer(editor,text_buffer_field);

    BeginScissorMode(BREAK_DOWN_RECT(render_field));
    size_t line_y = 0;
    for (size_t i = buffer->line_anchor; i < min(buffer->line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = MeasureTextEx(editor->settings.editor_font, number_str, editor->settings.font_size, 1);
        local_offset = measured_text.x;
        DrawTextEx(editor->settings.editor_font, number_str, (Vector2){render_field.position.x + max_offset - editor->settings.number_padding - local_offset, render_field.position.y + line_y * editor->settings.font_size}, editor->settings.font_size, 1, editor->settings.scheme.line_number_color);
        line_y++;
    }
    EndScissorMode();
    free(number_str);
}

void EditorRenderMode(Editor* editor) {
    char* mode;
    if (editor->input_system.current_mode == MODE_COMMAND) {
        mode = "Command Mode";
    } else {
        mode = "Text Mode";
    }
    DrawTextEx(editor->settings.editor_font, mode, PositionToVector(editor->settings.mode_padding), editor->settings.font_size, 1, editor->settings.scheme.mode_color);
}

void EditorRender(Editor* editor) {
    ClearBackground(editor->settings.scheme.background_color);
    EditorRenderMode(editor);
    EditorRenderTextField(editor, GetEditorTextFieldSize(editor));
    EditorRenderBar(editor);
    ModalSystemRender(editor);
}
