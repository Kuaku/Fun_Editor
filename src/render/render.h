#ifndef RENDER_H
#define RENDER_H

#include "../editor/editor.h"

void ClearEditorSettings(EditorSettings* settings);

size_t GetPointerOffsetFromLeft(Editor* editor, TextBuffer* buffer, Position pointer);

void RenderLineBufferWithSelection(Editor* editor, TextBuffer* buffer, char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position);
void RenderLineBuffer(Editor* editor, TextBuffer* buffer, char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position);
void RenderLine(Editor* editor, TextBuffer* buffer, int y_line, Position position, size_t index, Position pointer, Position selection_start_position, Position selection_end_position);

void EditorRenderTextBuffer(Editor* editor, Rect render_field);
void EditorRenderTextField(Editor* editor, Rect render_field);
void EditorRenderMode(Editor* editor);
void EditorRenderBar(Editor* editor);
void EditorRenderStatusBar(Editor* editor);
void EditorRenderCommand(Editor* editor);
void EditorRender(Editor* editor);

#endif
