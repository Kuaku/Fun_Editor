#ifndef EDITOR_H
#define EDITOR_H

#include "../common.h"
#include "../text_buffer/text_buffer.h"
#include "../input/input.h"
#include "../modal/modal.h"
#include "../filesystem/filesystem.h"
#include "../statistics/statistics.h"
#include "../render/render_system.h"

typedef struct {
    char* root_dir;

    TextBuffer* text_buffers;
    size_t text_buffers_capacity;
    size_t text_buffers_count;

    int open_text_buffer_index;
    bool exit_requested;
} EditorState;

typedef struct {
    RenderColor background_color;
    RenderColor mode_color;
    Color text_color;
    Color command_color;
    Color line_number_color;
    Color command_background_color;
    RenderColor selection_background_color;
    RenderColor selection_foreground_color;
} ColorScheme;

typedef struct {
    ColorScheme scheme;
    Font editor_font;
    Position pointer_padding;
    Position mode_padding;
    Position command_padding;
    size_t number_padding;
    size_t pointer_width;
    size_t font_size;
    char* font_path;
    int font_loading_size;

    // Only for initial set
    // TODO: Remember when implementing settings this is only initial
    size_t key_repeat_delay;
    size_t key_repeat_interval;
} EditorSettings;

struct Editor {
    EditorState state;
    EditorSettings settings;
    InputSystem input_system;
    ModalSystem modal_system;
    FileSystem file_system;
    StatisticSystem statistic_system;
    RenderSystem render_system;
};

EditorState InitEditorState(size_t capacity);
void ClearEditorState(EditorState* state);

void ResizeTextBuffers(EditorState* state);
size_t GetFreeTextBufferIndex(EditorState* state);

void CreateEditor(Editor* editor, EditorSettings settings, char* path);
void ClearEditor(Editor* editor);

TextBuffer* GetActiveBuffer(Editor* editor);
bool ShouldEditorClose(Editor* editor);

void OpenEmptyBuffer(Editor* editor);
void OpenFileFromPath(Editor* editor, const char* path);
void OpenDirectoryFromPath(Editor* editor, const char* path);
int FindBufferByPath(Editor* editor, const char* path);
void OpenOrSwitchToFile(Editor* editor, const char* path);

void MovePointerLeft(TextBuffer* buffer);
void MovePointerRight(TextBuffer* buffer);
void MovePointerUp(TextBuffer* buffer);
void MovePointerDown(TextBuffer* buffer);
void MovePointerWordLeft(TextBuffer* buffer);
void MovePointerWordRight(TextBuffer* buffer);

void MovePointerAction(Editor* editor, void(*move_function)(TextBuffer* buffer));
void MovePointerSelectionAction(Editor* editor, void(*move_function)(TextBuffer* buffer));

void InsertStringAction(Editor* editor, char* value, size_t len);
void RemoveBackwardsAction(Editor* editor);
void RemoveForwardAction(Editor* editor);
void InsertNewLineAction(Editor* editor);
void InsertTabAction(Editor* editor);

void UndoAction(Editor* editor);
void RedoAction(Editor* editor);

void CopyAction(Editor* editor);
void CutAction(Editor* editor);
void SelectAllAction(Editor* editor);
void PasteAction(Editor* editor);

void ToggleCommandModeAction(Editor* editor);
void SetCommandMode(Editor* editor, bool is_command_mode);

bool SaveActiveTextBuffer(Editor* editor);
void SaveAction(Editor* editor);

void DispatchInputTextMode(Editor* editor, Action action);
void DispatchInputCommandMode(Editor* editor, Action action);

void EditorHandleInput(Editor* editor);
void EditorHandleUpdate(Editor* editor);

void FileSystemUpdate(Editor* editor);

#endif
