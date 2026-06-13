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

void EditorRenderRoot(Editor* editor, RenderNode* self) {
    PushScissor(&editor->render_system.render_queue, self->inner_bounds);
    PushRect(&editor->render_system.render_queue, self->inner_bounds, editor->settings.scheme.background_color);
    PushScissorPop(&editor->render_system.render_queue);
}

void EditorRenderHeader(Editor* editor, RenderNode* self) {
    PushScissor(&editor->render_system.render_queue, self->inner_bounds);
    char* mode;
    if (editor->input_system.current_mode == MODE_COMMAND) {
        mode = "Command Mode";
    } else {
        mode = "Text Mode";
    }
    PushText(&editor->render_system.render_queue, mode, self->inner_bounds.position, editor->settings.font_size, 1, editor->settings.scheme.mode_color);
    PushScissorPop(&editor->render_system.render_queue);
}

int EditorMeasureGutter(Editor* editor, Rect parent) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    Position pointer = GetPointerPosition(buffer);
    size_t pointer_offset = GetPointerOffsetFromLeft(editor, buffer, pointer);
    size_t lines_completly_rendered = parent.size.y / editor->settings.font_size;
    size_t line_number = buffer->line_anchor;
    size_t line_count = GetLineCount(buffer);

    size_t max_offset = 0;
    size_t digits = snprintf(NULL, 0, "%zu", min(buffer->line_anchor + lines_completly_rendered + 1, line_count) + 1);
    size_t local_offset = 0;
    Position measured_text;
    char* number_str  = malloc(digits + 1);
    for (size_t i = buffer->line_anchor; i < min(buffer->line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, number_str, editor->settings.font_size, 1);
        local_offset = measured_text.x;
        if (local_offset > max_offset) {
            max_offset = local_offset;
        }
    }
    free(number_str);
    return max_offset;
}

void EditorRenderGutter(Editor* editor, RenderNode* self) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    PushScissor(&editor->render_system.render_queue, self->inner_bounds);
    size_t lines_completly_rendered = self->inner_bounds.size.y / editor->settings.font_size;
    size_t line_count = GetLineCount(buffer);
    size_t digits = snprintf(NULL, 0, "%zu", min(buffer->line_anchor + lines_completly_rendered + 1, line_count) + 1);
    char* number_str  = malloc(digits + 1);
    Position measured_text;
    size_t local_offset = 0;
    size_t line_y = 0;
    for (size_t i = buffer->line_anchor; i < min(buffer->line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, number_str, editor->settings.font_size, 1);
        local_offset = measured_text.x;
        PushText(&editor->render_system.render_queue, number_str, (Position){self->inner_bounds.position.x + self->inner_bounds.size.x - local_offset, self->inner_bounds.position.y + line_y * editor->settings.font_size}, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.line_number_color.r, editor->settings.scheme.line_number_color.g, editor->settings.scheme.line_number_color.b, editor->settings.scheme.line_number_color.a});
        line_y++;
    }
    free(number_str);
    PushScissorPop(&editor->render_system.render_queue);
}

static void PushTextSegment(Editor* editor, const char* segment, int col_offset, int base_x, int draw_y, int line_index, bool has_selection, Position selection_start, Position selection_end, RenderColor text_color) {
    RenderQueue* queue = &editor->render_system.render_queue;
    RenderWrapper* wrapper = &editor->render_system.render_wrapper;
    size_t font_size = editor->settings.font_size;
    int seg_len = (int)strlen(segment);

    PushText(queue, segment, (Position){base_x, draw_y}, font_size, 1, text_color);

    if (!has_selection || line_index < selection_start.y || line_index > selection_end.y) return;

    int line_sel_start = (line_index == selection_start.y) ? selection_start.x : 0;
    int line_sel_end   = (line_index == selection_end.y)   ? selection_end.x   : col_offset + seg_len;

    int local_start = line_sel_start - col_offset;
    int local_end   = line_sel_end - col_offset;
    if (local_start < 0) local_start = 0;
    if (local_end > seg_len) local_end = seg_len;
    if (local_end <= local_start) return;

    char* before = calloc(local_start + 1, sizeof(char));
    strncpy(before, segment, local_start);
    int before_width = wrapper->measure_text(wrapper, before, font_size, 1).x;
    free(before);

    char* selected = calloc(local_end - local_start + 1, sizeof(char));
    strncpy(selected, segment + local_start, local_end - local_start);
    int selected_width = wrapper->measure_text(wrapper, selected, font_size, 1).x;

    PushRect(queue, (Rect){{base_x + before_width, draw_y}, {selected_width, font_size}}, editor->settings.scheme.selection_background_color);
    PushText(queue, selected, (Position){base_x + before_width, draw_y}, font_size, 1, editor->settings.scheme.selection_foreground_color);
    free(selected);
}

void EditorRenderTextFieldNew(Editor* editor, RenderNode* self) {
    RenderQueue* queue = &editor->render_system.render_queue;
    RenderWrapper* wrapper = &editor->render_system.render_wrapper;
    TextBuffer* buffer = GetActiveBuffer(editor);
    Rect field = self->inner_bounds;

    Color tc = editor->settings.scheme.text_color;
    RenderColor text_color = (RenderColor){tc.r, tc.g, tc.b, tc.a};

    size_t font_size = editor->settings.font_size;
    size_t line_count = GetLineCount(buffer);
    size_t lines_completly_rendered = field.size.y / font_size;

    Position pointer = GetPointerPosition(buffer);

    Position selection_start = {0};
    Position selection_end = {0};
    if (buffer->has_selection) {
        selection_start = IndexToPosition(buffer, min(buffer->selection_start, buffer->selection_end));
        selection_end = IndexToPosition(buffer, max(buffer->selection_start, buffer->selection_end));
    }

    char* pointer_line = GenerateLine(buffer, pointer.y);
    char* prefix = calloc(pointer.x + 1, sizeof(char));
    strncpy(prefix, pointer_line, pointer.x);
    size_t pointer_offset = wrapper->measure_text(wrapper, prefix, font_size, 1).x;
    free(prefix);
    free(pointer_line);

    if (pointer.y >= buffer->line_anchor + lines_completly_rendered) {
        buffer->line_anchor = pointer.y - lines_completly_rendered + 1;
    }
    if (pointer.y <= buffer->line_anchor) {
        buffer->line_anchor = pointer.y;
    }

    int cursor_advance = editor->settings.pointer_padding.x * 2 + editor->settings.pointer_width;
    if (buffer->offset_x + field.size.x <= pointer_offset) {
        buffer->offset_x = pointer_offset - field.size.x + cursor_advance;
    }
    if (buffer->offset_x > pointer_offset) {
        buffer->offset_x = pointer_offset;
    }

    PushScissor(queue, field);

    size_t last_line = min(buffer->line_anchor + lines_completly_rendered + 1, line_count);
    size_t line_y = 0;
    for (size_t i = buffer->line_anchor; i < last_line; ++i) {
        char* line = GenerateLine(buffer, i);
        int draw_x = field.position.x - buffer->offset_x;
        int draw_y = field.position.y + line_y * font_size;

        if ((int)i != pointer.y) {
            PushTextSegment(editor, line, 0, draw_x, draw_y, (int)i, buffer->has_selection, selection_start, selection_end, text_color);
        } else {
            char* prefix = calloc(pointer.x + 1, sizeof(char));
            strncpy(prefix, line, pointer.x);
            PushTextSegment(editor, prefix, 0, draw_x, draw_y, (int)i, buffer->has_selection, selection_start, selection_end, text_color);
            free(prefix);

            int cursor_x = draw_x + pointer_offset + editor->settings.pointer_padding.x;
            PushRect(queue, (Rect){{cursor_x, draw_y + editor->settings.pointer_padding.y}, {editor->settings.pointer_width, font_size - 2 * editor->settings.pointer_padding.y}}, text_color);

            int suffix_x = draw_x + pointer_offset + cursor_advance;
            PushTextSegment(editor, line + pointer.x, pointer.x, suffix_x, draw_y, (int)i, buffer->has_selection, selection_start, selection_end, text_color);
        }

        free(line);
        line_y++;
    }

    PushScissorPop(queue);
}

int EditorMeasurePointerBarSize(Editor* editor, Rect parent) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    Position cursor = GetPointerCodePosition(buffer);
    int length = snprintf(NULL, 0, "%d | %d", cursor.x + 1, cursor.y + 1);
    char* position_str = (char*)malloc(length + 1);
    snprintf(position_str, length + 1, "%d | %d", cursor.x + 1, cursor.y + 1);
    Position bounding_position_str = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, position_str, editor->settings.font_loading_size, 1);
    free(position_str);
    return bounding_position_str.x;
}

void RenderCommandBarNew(Editor* editor, RenderNode* self) {
    PushScissor(&editor->render_system.render_queue, self->inner_bounds);
    PushText(&editor->render_system.render_queue, ":", self->inner_bounds.position, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    Position offset_prefix = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, ": ", editor->settings.font_size, 1);
    
    char* temp;
    temp = calloc(editor->input_system.command_system.pointer_position + 1, sizeof(char));
    strncpy(temp, editor->input_system.command_system.command_buffer, editor->input_system.command_system.pointer_position);
    PushText(&editor->render_system.render_queue, temp, (Position){self->inner_bounds.position.x + offset_prefix.x, self->inner_bounds.position.y}, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    Position offset_first_part = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, temp, editor->settings.font_size, 1);
    PushRect(&editor->render_system.render_queue, (Rect){{self->inner_bounds.position.x + offset_prefix.x + offset_first_part.x + editor->settings.pointer_padding.x, self->inner_bounds.position.y}, {editor->settings.pointer_width, editor->settings.font_size}}, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    char* last_part = editor->input_system.command_system.command_buffer + editor->input_system.command_system.pointer_position;
    PushText(&editor->render_system.render_queue, last_part, (Position){self->inner_bounds.position.x + offset_prefix.x + offset_first_part.x + editor->settings.pointer_padding.x * 2 + editor->settings.pointer_width, self->inner_bounds.position.y}, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    free(temp);

    PushScissorPop(&editor->render_system.render_queue);
}

void EditorRenderPointerBar(Editor* editor, RenderNode* self) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    Position cursor = GetPointerCodePosition(buffer);
    int length = snprintf(NULL, 0, "%d | %d", cursor.x + 1, cursor.y + 1);
    char* position_str = (char*)malloc(length + 1);
    snprintf(position_str, length + 1, "%d | %d", cursor.x + 1, cursor.y + 1);
    Position bounding_position_str = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, position_str, editor->settings.font_loading_size, 1);
    PushText(&editor->render_system.render_queue, position_str, self->inner_bounds.position, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    free(position_str);
}

void EditorRenderSeperater(Editor* editor, RenderNode* self) {
    PushRect(&editor->render_system.render_queue, self->inner_bounds, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
}

void EditorRenderFileBar(Editor* editor, RenderNode* self) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    const char* full_path = buffer->file_path;
    const char* label = full_path ? full_path : "[untitled]";
    Position label_size = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, label, editor->settings.font_size, 1);
    Position marker_size = {0};
    bool dirty = IsBufferDirty(buffer);
    int path_avail = self->inner_bounds.size.x;
    if (dirty) {
        marker_size = editor->render_system.render_wrapper.measure_text(&editor->render_system.render_wrapper, "\u25CF", editor->settings.font_size, 1);
        path_avail -= (int)marker_size.x + editor->settings.command_padding.x;
    }
    if (path_avail < 0) path_avail = 0;

    int scissor_w = min((int)label_size.x, path_avail);
    PushScissor(&editor->render_system.render_queue, (Rect){self->inner_bounds.position, {scissor_w, self->inner_bounds.size.y}});
    PushText(&editor->render_system.render_queue, label, (Position){self->inner_bounds.position.x - (label_size.x - scissor_w), self->inner_bounds.position.y}, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    PushScissorPop(&editor->render_system.render_queue);

    if (dirty) {
        int marker_x = self->inner_bounds.position.x + scissor_w + editor->settings.command_padding.x;
        PushText(&editor->render_system.render_queue, "\u25CF", (Position){marker_x, self->inner_bounds.position.y}, editor->settings.font_size, 1, (RenderColor){editor->settings.scheme.command_color.r, editor->settings.scheme.command_color.g, editor->settings.scheme.command_color.b, editor->settings.scheme.command_color.a});
    }
}

void EditorBuildRenderTree(Editor* editor) {
    RenderSystem* system = &editor->render_system;
    RenderWrapper* wrapper = &system->render_wrapper;
    Position screen_size = {
        wrapper->get_screen_width(wrapper),
        wrapper->get_screen_height(wrapper)
    };

    NodeHandle root = VNODE(system, .horizontal_axis_type = AXIS_FIXED, .vertical_axis_type = AXIS_FIXED, .fixed_size = screen_size, .custom_render = EditorRenderRoot);
    NodeHandle header = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_FIXED, .fixed_size = {0, editor->settings.font_size + 2 * 5}, .padding = {{5, 5}, {5, 5}}, .custom_render = EditorRenderHeader);
    
    NodeHandle main_editor = HNODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_GROW, .padding = {{5, 5}, {5, 5}});
    NodeHandle gutter = NODE(system, .horizontal_axis_type = AXIS_MEASURE_SECOND, .custom_measure_x =  EditorMeasureGutter, .vertical_axis_type = AXIS_GROW, .custom_render = EditorRenderGutter, .margin = {{5, 0}, {5, 0}});
    NodeHandle text_field = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_GROW, .custom_render = EditorRenderTextFieldNew);
    AppendChild(&system->tree_holder, main_editor, gutter);
    AppendChild(&system->tree_holder, main_editor, text_field);
    
    NodeHandle footer = HNODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_FIXED, .fixed_size = {0, editor->settings.font_size + 2 * 5}, .padding = {{5, 5}, {5, 5}});

    if (editor->input_system.current_mode == MODE_COMMAND) {
        NodeHandle command_bar = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_GROW, .custom_render = RenderCommandBarNew);
        AppendChild(&system->tree_holder, footer, command_bar);
    } else {
        NodeHandle file_bar = NODE(system, .horizontal_axis_type = AXIS_GROW, .vertical_axis_type = AXIS_GROW, .custom_render = EditorRenderFileBar);
        NodeHandle seperater = NODE(system, .horizontal_axis_type = AXIS_FIXED, .vertical_axis_type = AXIS_GROW, .fixed_size = {2, 0}, .custom_render = EditorRenderSeperater, .margin = {{5, 0}, {5, 0}}, .padding = {{0, 2}, {0, 2}});
        NodeHandle pointer_bar = NODE(system, .horizontal_axis_type = AXIS_MEASURE_SECOND, .custom_measure_x = EditorMeasurePointerBarSize, .vertical_axis_type = AXIS_GROW, .custom_render = EditorRenderPointerBar, .margin = {{0, 0}, {5, 0}});
        AppendChild(&system->tree_holder, footer, file_bar);
        AppendChild(&system->tree_holder, footer, seperater);
        AppendChild(&system->tree_holder, footer, pointer_bar);
    }
    

    AppendChild(&system->tree_holder, root, header);
    AppendChild(&system->tree_holder, root, main_editor);
    AppendChild(&system->tree_holder, root, footer);
}

void EditorRender(Editor* editor) {
    TreeReset(&editor->render_system.tree_holder);
    EditorBuildRenderTree(editor);
    CalculateSizingModel(editor, &editor->render_system);
    GenerateRenderList(&editor->render_system);

    ResetRenderQueue(&editor->render_system.render_queue);
    for (size_t i = 0; i < editor->render_system.tree_holder.traversal_list.count; i++) {
        NodeHandle node_handle = editor->render_system.tree_holder.traversal_list.order[i];
        RenderNode* node = &editor->render_system.tree_holder.nodes[node_handle];
        if (node->custom_render) {
            node->custom_render(editor, node);
        }
    }
    editor->render_system.render_wrapper.render_queue(&editor->render_system.render_wrapper, &editor->render_system.render_queue);

    //ClearBackground(editor->settings.scheme.background_color);
    //EditorRenderMode(editor);
    //EditorRenderTextField(editor, GetEditorTextFieldSize(editor));
    //EditorRenderBar(editor);
    ModalSystemRender(editor);
}


