#ifndef RENDER_H
#define RENDER_H

#include "../editor/editor.h"

void ClearEditorSettings(EditorSettings* settings);
size_t GetPointerOffsetFromLeft(Editor* editor, TextBuffer* buffer, Position pointer);

void EditorRenderRoot(Editor* editor, RenderNode* self);
void EditorRenderHeader(Editor* editor, RenderNode* self);
int EditorMeasureGutter(Editor* editor, Rect parent);
void EditorRenderGutter(Editor* editor, RenderNode* self);
void EditorRenderTextFieldNew(Editor* editor, RenderNode* self);
int EditorMeasurePointerBarSize(Editor* editor, Rect parent);
void RenderCommandBarNew(Editor* editor, RenderNode* self);
void EditorRenderPointerBar(Editor* editor, RenderNode* self);
void EditorRenderSeperater(Editor* editor, RenderNode* self);
void EditorRenderFileBar(Editor* editor, RenderNode* self);
void EditorBuildRenderTree(Editor* editor);
void EditorRender(Editor* editor);

#endif
