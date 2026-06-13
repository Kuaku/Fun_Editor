#include "editor.h"
#include "../render/render.h"
#include "./modal/modals/buffer_list.h"
#include "./modal/modals/file_explorer.h"
#include "./modal/modals/statistics_modal.h"
#include "../render/raylib_wrapper.h"

EditorState InitEditorState(size_t capacity) {
    EditorState state = {0};
    state.root_dir = NULL;
    state.open_text_buffer_index = -1;
    state.text_buffers = calloc(capacity, sizeof(TextBuffer));
    state.text_buffers_capacity = capacity;
    state.text_buffers_count = 0;
    return state;
}

void ClearEditorState(EditorState* state) {
    if (!state) return;

    if (state->root_dir) {
        free(state->root_dir);
        state->root_dir = NULL;
    }

    if (state->text_buffers) {
        for (size_t i = 0; i < state->text_buffers_count; i++) {
            ClearTextBuffer(&state->text_buffers[i]);
        }
        free(state->text_buffers);
        state->text_buffers = NULL;
    }
    state->text_buffers_capacity = 0;
    state->text_buffers_count = 0;
    state->open_text_buffer_index = 0;

    state->exit_requested = false;
}

void ResizeTextBuffers(EditorState* state) {
    size_t new_size = state->text_buffers_capacity * 2;
    state->text_buffers = realloc(state->text_buffers, new_size * sizeof(TextBuffer));
    state->text_buffers_capacity = new_size;
}

size_t GetFreeTextBufferIndex(EditorState* state) {
    size_t index = state->text_buffers_count++;
    while (index >= state->text_buffers_capacity) {
        ResizeTextBuffers(state);
    }
    return index;
}

void CreateEditor(Editor* editor, EditorSettings settings, char* path) {
    editor->settings = settings;
    editor->state = InitEditorState(INITIAL_TEXT_BUFFER_CAPACITY);

    FileType root_type = TYPE_ERROR;
    if (path) {
        root_type = GetFileTypeFromPath(path);
    }

    RenderWrapper raylib_wrapper = CreateRaylibRenderWrapper(settings.font_path, settings.font_loading_size);

    editor->render_system = InitRenderSystem(raylib_wrapper);
    editor->input_system = InitInputSystem(settings.key_repeat_delay, settings.key_repeat_interval);
    editor->modal_system = InitModalSystem();
    editor->file_system = InitFileSystem();
    editor->statistic_system = InitStatisticSystem();

    if (root_type == TYPE_FILE) {
        OpenFileFromPath(editor, path);
    } else if (root_type == TYPE_DIR) {
        OpenDirectoryFromPath(editor, path);
    } else {
        OpenEmptyBuffer(editor);
    }
}

void ClearEditor(Editor* editor) {
    if (!editor) return;

    ClearEditorState(&editor->state);
    ClearEditorSettings(&editor->settings);
    ClearRenderSystem(&editor->render_system);
    ClearInputSystem(&editor->input_system);
    ClearModalSystem(&editor->modal_system);
    ClearFileSystem(&editor->file_system);
    ClearStatisticsSystem(&editor->statistic_system);
}

TextBuffer* GetActiveBuffer(Editor* editor) {
    return &editor->state.text_buffers[editor->state.open_text_buffer_index];
}

bool ShouldEditorClose(Editor* editor) {
    return editor->state.exit_requested;
}

Rect GetEditorTextFieldSize(Editor* editor) {
    return (Rect){
        .position = (Position){0, editor->settings.mode_padding.x * 2 + editor->settings.font_size},
        .size = (Position){GetScreenWidth() - 40, GetScreenHeight() - (editor->settings.mode_padding.y * 2 + editor->settings.font_size * 2 + editor->settings.command_padding.y * 2)}
    };
}

void OpenEmptyBuffer(Editor* editor) {
    EditorState* state = &editor->state;
    size_t index = GetFreeTextBufferIndex(state);

    InitEmptyTextBuffer(&state->text_buffers[index]);
    state->open_text_buffer_index = index;
}

void OpenDirectoryFromPath(Editor* editor, const char* path) {
    FileSystemBuild(&editor->file_system, path);
    OpenEmptyBuffer(editor);
}

void OpenFileFromPath(Editor* editor, const char* path) {
    EditorState* state = &editor->state;
    size_t index = GetFreeTextBufferIndex(state);

    InitTextBufferFromPath(&state->text_buffers[index], path);
    state->open_text_buffer_index = index;
}

int FindBufferByPath(Editor* editor, const char* path) {
    EditorState* state = &editor->state;
    for (size_t i = 0; i < state->text_buffers_count; i++) {
        if (state->text_buffers[i].file_path && strcmp(state->text_buffers[i].file_path, path) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void OpenOrSwitchToFile(Editor* editor, const char* path) {
    int existing = FindBufferByPath(editor, path);
    if (existing >= 0) {
        editor->state.open_text_buffer_index = existing;
    } else {
        OpenFileFromPath(editor, path);
    }
}

void MovePointerLeft(TextBuffer* buffer) {
    if (buffer->pointer_position > 0) {
        buffer->pointer_position--;
        while (buffer->pointer_position > 0 && IsContinuationByte(buffer, buffer->pointer_position)) {
            buffer->pointer_position--;
        }
        buffer->request_revalidate_pointer_cache = true;
    }
}

void MovePointerRight(TextBuffer* buffer) {
    size_t text_buffer_size = GetTextSize(buffer);
    if (buffer->pointer_position < text_buffer_size) {
        buffer->pointer_position++;
        while (buffer->pointer_position <= text_buffer_size && IsContinuationByte(buffer, buffer->pointer_position)) {
            buffer->pointer_position++;
        }
        buffer->request_revalidate_pointer_cache = true;
    }
}

void MovePointerUp(TextBuffer* buffer) {
    Position pointer = GetPointerCodePosition(buffer);
    if (pointer.y == 0) {
        return;
    }
    Position nextLine = GetLineByIndex(buffer, pointer.y - 1);

    size_t target_col = pointer.x;
    size_t byte_offset = 0;
    size_t col = 0;
    while (col < target_col && byte_offset < nextLine.y) {
        uint32_t cp;
        size_t cp_len = GetCodepointAt(buffer, nextLine.x + byte_offset, &cp);
        if (cp == '\n' || cp_len == 0) break;
        byte_offset += cp_len;
        col++;
    }

    size_t new_pos = nextLine.x + byte_offset;
    if (buffer->pointer_position != new_pos) {
        buffer->pointer_position = new_pos;
        buffer->request_revalidate_pointer_cache = true;
    }
}

void MovePointerDown(TextBuffer* buffer) {
    Position pointer = GetPointerCodePosition(buffer);
    size_t max_lines = GetLineCount(buffer);
    if (pointer.y >= max_lines - 1) {
        return;
    }
    Position nextLine = GetLineByIndex(buffer, pointer.y + 1);

    size_t target_col = pointer.x;
    size_t byte_offset = 0;
    size_t col = 0;
    while (col < target_col && byte_offset < nextLine.y) {
        uint32_t cp;
        size_t cp_len = GetCodepointAt(buffer, nextLine.x + byte_offset, &cp);
        if (cp == '\n' || cp_len == 0) break;
        byte_offset += cp_len;
        col++;
    }

    size_t new_pos = nextLine.x + byte_offset;
    if (buffer->pointer_position != new_pos) {
        buffer->pointer_position = new_pos;
        buffer->request_revalidate_pointer_cache = true;
    }
}

void MovePointerWordRight(TextBuffer* buffer) {
    size_t size = GetTextSize(buffer);
    if (buffer->pointer_position >= size) return;
    uint32_t codepoint;
    size_t utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);

    if (codepoint == '\n') {
        buffer->pointer_position += utf8_length;
        return;
    }

    if (IsWordChar(codepoint)) {
        while (buffer->pointer_position < size) {
            utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!IsWordChar(codepoint)) break;
            buffer->pointer_position += utf8_length;
        }
    } else if (IsPunct(codepoint)) {
        while (buffer->pointer_position < size) {
            utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!IsPunct(codepoint)) break;
            buffer->pointer_position += utf8_length;
        }
    } else {
        while (buffer->pointer_position < size) {
            utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!(codepoint == ' ' || codepoint == '\t')) break;
            buffer->pointer_position += utf8_length;
        }
        if (buffer->pointer_position < size) return;
        utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
        if (codepoint == '\n') return;
        
        while (buffer->pointer_position < size) {
            utf8_length = GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!IsPunct(codepoint)) break;
            buffer->pointer_position += utf8_length;
        }
    }
}

void MovePointerWordLeft(TextBuffer* buffer) {
    if (buffer->pointer_position == 0) return;

    MovePointerLeft(buffer);

    uint32_t codepoint;
    GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
    if (codepoint == '\n') return;

    while (buffer->pointer_position > 0) {
        GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
        if (!(codepoint == ' ' || codepoint == '\t')) break;
        size_t prev = buffer->pointer_position;
        MovePointerLeft(buffer);
        GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
        if (codepoint == '\n') {
            buffer->pointer_position = prev;
            return;
        }
    }

    GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
    if (IsWordChar(codepoint)) {
        while (buffer->pointer_position > 0) {
            size_t prev = buffer->pointer_position;
            MovePointerLeft(buffer);
            GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!IsWordChar(codepoint)) {
                buffer->pointer_position = prev;
                break;
            }
        }
    } else if (IsPunct(codepoint)) {
        while (buffer->pointer_position > 0) {
            size_t prev = buffer->pointer_position;
            MovePointerLeft(buffer);
            GetCodepointAt(buffer, buffer->pointer_position, &codepoint);
            if (!IsPunct(codepoint)) {
                buffer->pointer_position = prev;
                break;
            }
        }
    }
}

void MovePointerAction(Editor* editor, void(*move_function)(TextBuffer* buffer)) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    size_t pointer_before = buffer->pointer_position;
    move_function(buffer);
    if (buffer->pointer_position != pointer_before) {
        buffer->has_selection = false;
    }
}

void MovePointerSelectionAction(Editor* editor, void(*move_function)(TextBuffer* buffer)) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    size_t pointer_before = buffer->pointer_position;
    move_function(buffer);
    if (pointer_before != buffer->pointer_position) {
        if (!buffer->has_selection) {
            buffer->has_selection = true;
            buffer->selection_start = pointer_before;
        }
        buffer->selection_end = buffer->pointer_position;
    }
}

void InsertStringAction(Editor* editor, char* value, size_t len) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (buffer->has_selection) {
        RemoveSelection(buffer);
    }
    double current_time = GetTime();
    if (!TryToMergeCharacterInsert(buffer, value, len, current_time)) {
        char* text = malloc(len + 1);
        memcpy(text, value, len);
        text[len] = '\0';
        PushCommand(buffer, EDIT_INSERT, buffer->pointer_position, text, len);
        free(text);
    }
    InsertString(buffer, buffer->pointer_position, value, len);
    buffer->pointer_position += len;
    buffer->time_since_last_edit = current_time;
}

void RemoveBackwardsAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (buffer->has_selection) {
        RemoveSelection(buffer);
    } else {
        if (buffer->pointer_position == 0) return;

        double current_time = GetTime();

        size_t start_byte_position = buffer->pointer_position - 1;
        while (start_byte_position > 0 && IsContinuationByte(buffer, start_byte_position)) {
            start_byte_position--;
        }

        if (!TryToMergeCharacterRemove(buffer, current_time)) {
            char* utf8_buffer = GetTextRangeRaw(buffer, start_byte_position, buffer->pointer_position);
            PushCommand(buffer, EDIT_DELETE, start_byte_position, utf8_buffer, buffer->pointer_position - start_byte_position);
            free(utf8_buffer);
        }

        ExecuteDelete(buffer, start_byte_position, buffer->pointer_position - start_byte_position);
        buffer->time_since_last_edit = current_time;
    }
}

void RemoveForwardAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (buffer->has_selection) { RemoveSelection(buffer); return; }
    if (buffer->pointer_position >= GetTextSize(buffer)) return;
    size_t end = buffer->pointer_position + 1;
    while (end < GetTextSize(buffer) && IsContinuationByte(buffer, end)) end++;
    RemoveArea(buffer, buffer->pointer_position, end - buffer->pointer_position);
}

void InsertNewLineAction(Editor* editor) {
    char new_line_buffer[1];
    new_line_buffer[0] = '\n';
    InsertStringAction(editor, new_line_buffer, 1);
}

void InsertTabAction(Editor* editor) {
    char tab_buffer[2];
    tab_buffer[0] = ' ';
    tab_buffer[1] = ' ';
    InsertStringAction(editor, tab_buffer, 2);
}

void UndoAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    UndoStack* stack = &buffer->undo_stack;
    if (stack->current == 0) return;

    stack->current--;
    EditEntry* entry = &stack->entries[stack->current];

    switch (entry->type)
    {
        case EDIT_INSERT:
            ExecuteDelete(buffer, entry->position, entry->length);
            break;
        case EDIT_DELETE:
            InsertString(buffer, entry->position, entry->text, entry->length);
            break;
    }

    buffer->pointer_position = entry->cursor_before;
    buffer->line_cache.is_valid = false;
}

void RedoAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    UndoStack* stack = &buffer->undo_stack;
    if (stack->current >= stack->count) return;

    EditEntry* entry = &stack->entries[stack->current];

    switch (entry->type) {
        case EDIT_INSERT: {
            InsertString(buffer, entry->position, entry->text, entry->length);
            break;
        }
        case EDIT_DELETE: {
            ExecuteDelete(buffer, entry->position, entry->length);
            break;
        }
    }

    buffer->pointer_position = entry->cursor_after;
    stack->current++;
    buffer->line_cache.is_valid = false;
}

void PasteAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    const char* clipboard_text = GetClipboardText();
    if (clipboard_text == NULL || clipboard_text[0] == '\0') {
        return;
    }

    if (buffer->has_selection) {
        RemoveSelection(buffer);
    }

    size_t clipboard_length = strlen(clipboard_text);
    char* paste_buffer = malloc(clipboard_length + 1);
    if (paste_buffer == NULL) {
        return;
    }
    strcpy(paste_buffer, clipboard_text);

    normalize_line_endings(paste_buffer);
    size_t paste_buffer_length = strlen(paste_buffer);

    PushCommand(buffer, EDIT_INSERT, buffer->pointer_position, paste_buffer, paste_buffer_length);

    InsertString(buffer, buffer->pointer_position, paste_buffer, paste_buffer_length);
    buffer->pointer_position += paste_buffer_length;

    free(paste_buffer);
}

void CopyAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (!buffer->has_selection) return;
    size_t selection_length = abs((int)buffer->selection_end - (int)buffer->selection_start);
    char* selection_buffer = GetTextRange(buffer, min(buffer->selection_start, buffer->selection_end), min(buffer->selection_start, buffer->selection_end) + selection_length);

    SetClipboardText(selection_buffer);

    free(selection_buffer);
}

void CutAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (!buffer->has_selection) return;

    CopyAction(editor);
    RemoveSelection(buffer);
}

void SelectAllAction(Editor* editor) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    buffer->selection_start = 0;
    buffer->selection_end = GetTextSize(buffer);
    buffer->pointer_position = buffer->selection_end;
    buffer->has_selection = true;
}

void ToggleCommandModeAction(Editor* editor) {
    if (editor->input_system.current_mode == MODE_COMMAND) {
        SetCommandMode(editor, false);
    } else {
        SetCommandMode(editor, true);
    }
}

void SetCommandMode(Editor* editor, bool is_command_mode) {
    EditorMode before = editor->input_system.current_mode;
    if (is_command_mode) {
        editor->input_system.current_mode = MODE_COMMAND;
    } else {
        editor->input_system.current_mode = MODE_TEXT;
    }

    if (before != editor->input_system.current_mode) {
        InputSystemClearHeld(&editor->input_system);
    }
}

bool SaveActiveTextBuffer(Editor* editor) {
    TextBuffer* active_buffer = GetActiveBuffer(editor);
    if (!active_buffer->file_path) return false;

    size_t len;
    char* text = FlattenTextBuffer(active_buffer, &len);

    if (!text) return false;

    char tmp_path[PATH_MAX_LEN];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", active_buffer->file_path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        free(text);
        return false;
    }

    size_t written = fwrite(text, 1, len, f);
    fclose(f);
    free(text);

    if (written != len) {
        remove(tmp_path);
        return false;
    }

    #ifdef _WIN32
        if (!MoveFileExA(tmp_path, active_buffer->file_path, MOVEFILE_REPLACE_EXISTING)) {
            remove(tmp_path);
            return false;
        }
    #else
        if (rename(tmp_path, active_buffer->file_path) != 0) {
            remove(tmp_path);
            return false;
        }
    #endif

    active_buffer->undo_stack.saved_current = active_buffer->undo_stack.current;
    return true;
}

void SaveAction(Editor* editor) {
    bool is_saved = SaveActiveTextBuffer(editor);
}

void DispatchInputTextMode(Editor* editor, Action action) {
    switch (action.type)
    {
    case ACTION_CURSOR_LEFT:
        MovePointerAction(editor, MovePointerLeft);
        break;
    case ACTION_CURSOR_RIGHT:
        MovePointerAction(editor, MovePointerRight);
        break;
    case ACTION_CURSOR_DOWN:
        MovePointerAction(editor, MovePointerDown);
        break;
    case ACTION_CURSOR_UP:
        MovePointerAction(editor, MovePointerUp);
        break;
    case ACTION_CURSOR_WORD_RIGHT:
        MovePointerAction(editor, MovePointerWordRight);
        break;
    case ACTION_CURSOR_WORD_LEFT:
        MovePointerAction(editor, MovePointerWordLeft);
        break;
    case ACTION_SELECT_LEFT:
        MovePointerSelectionAction(editor, MovePointerLeft);
        break;
    case ACTION_SELECT_RIGHT:
        MovePointerSelectionAction(editor, MovePointerRight);
        break;
    case ACTION_SELECT_DOWN:
        MovePointerSelectionAction(editor, MovePointerDown);
        break;
    case ACTION_SELECT_UP:
        MovePointerSelectionAction(editor, MovePointerUp);
        break;
    case ACTION_SELECT_WORD_RIGHT:
        MovePointerSelectionAction(editor, MovePointerWordRight);
        break;
    case ACTION_SELECT_WORD_LEFT:
        MovePointerSelectionAction(editor, MovePointerWordLeft);
        break;
    case ACTION_INSERT_CHAR:
        InsertStringAction(editor, action.text_buffer, action.length);
        break;
    case ACTION_DELETE_BACKWARD:
        RemoveBackwardsAction(editor);
        break;
    case ACTION_DELETE_FORWARD:
        RemoveForwardAction(editor);
        break;
    case ACTION_INSERT_NEWLINE:
        InsertNewLineAction(editor);
        break;
    case ACTION_INSERT_TAB:
        InsertTabAction(editor);
        break;
    case ACTION_UNDO:
        UndoAction(editor);
        break;
    case ACTION_REDO:
        RedoAction(editor);
        break;
    case ACTION_PASTE:
        PasteAction(editor);
        break;
    case ACTION_COPY:
        CopyAction(editor);
        break;
    case ACTION_CUT:
        CutAction(editor);
        break;
    case ACTION_SELECT_ALL:
        SelectAllAction(editor);
        break;
    case ACTION_OPEN_COMMAND_PALETTE:
        ToggleCommandModeAction(editor);
        break;
    case ACTION_GOTO:
        EnterCommandModeWithCommand(editor, "goto ", strlen("goto "));
        break;
    case ACTION_QUIT:
        EnterCommandModeWithCommand(editor, "quit ", strlen("quit"));
        break;
    case ACTION_SEARCH:
        EnterCommandModeWithCommand(editor, "find \"\"", strlen("find \"\"") - 1);
        break;
    case ACTION_OPEN_BUFFER_LIST:
        OpenBufferListModal(editor);
        break;
    case ACTION_OPEN_FILE:
        EnterCommandModeWithCommand(editor, "open \"\"", strlen("open \"\"") - 1);
        break;
    case ACTION_OPEN_FILE_EXPLORER:
        OpenFileExplorerModal(editor);
        break;
    case ACTION_OPEN_STATISTICS:
        OpenStatisticsModal(editor);
        break;
    case ACTION_SAVE:
        SaveAction(editor);
        break;
    default:
        TraceLog(LOG_INFO, "ActionType: %s is not implemented for Text Mode", ActionTypeToString(action.type));
    }
}

void DispatchInputCommandMode(Editor* editor, Action action) {
    switch (action.type)
    {
    case ACTION_OPEN_COMMAND_PALETTE:
        ToggleCommandModeAction(editor);
        break;
    case ACTION_CANCEL:
        ToggleCommandModeAction(editor);
        break;
    case ACTION_CURSOR_LEFT:
        MoveCommandPointerLeft(&editor->input_system.command_system);
        break;
    case ACTION_CURSOR_RIGHT:
        MoveCommandPointerRight(&editor->input_system.command_system);
        break;
    case ACTION_DELETE_BACKWARD:
        CommandSystemBackspace(&editor->input_system.command_system);
        break;
    case ACTION_EXECUTE_COMMAND:
        TryExecuteCommandSystem(editor);
        break;
    case ACTION_INSERT_CHAR:
        CommandSystemInsertString(&editor->input_system.command_system, action.text_buffer, action.length);
        break;
    default:
        TraceLog(LOG_INFO, "ActionType: %s is not implemented for Command Mode", ActionTypeToString(action.type));
    }
}

void FileSystemUpdate(Editor* editor) {
    double current_time = GetTime();

    FileSystem* file_system = &editor->file_system;
    if (file_system->poll_state.in_progress) {
        TimerStart(&editor->statistic_system, FILE_POLLING_STEP_TIMER);
        if (FileSystemPollStep(file_system, INITIAL_DIRS_PER_FRAME)) {
            TimerEnd(&editor->statistic_system, FILE_POLLING_TIMER);
        }
        TimerEnd(&editor->statistic_system, FILE_POLLING_STEP_TIMER);
    } else if (strlen(file_system->root_path) > 0 &&
               current_time - file_system->last_scan_time > file_system->scan_interval) {
                file_system->poll_state.request_validation = true;
    }

    if (file_system->poll_state.request_validation && !file_system->poll_state.in_progress) {
        file_system->poll_state.request_validation = false;
        file_system->last_scan_time = current_time;
        TimerStart(&editor->statistic_system, FILE_POLLING_TIMER);
        FileSystemPollBegin(file_system);
    }
}

void EditorHandleUpdate(Editor* editor) {
    FileSystemUpdate(editor);

    TimerStart(&editor->statistic_system, MODAL_UPDATE);
    ModalSystem* system = &editor->modal_system;

    for (size_t i = 0; i < system->stack_count; i++) {
        if (system->stack[i] && system->stack[i]->custom_update) {
            Modal* modal = system->stack[i];
            system->stack[i]->custom_update(modal);
        }
    }
    TimerEnd(&editor->statistic_system, MODAL_UPDATE);

    if (!editor->file_system.poll_state.in_progress) {
        editor->file_system.dirty = false;
    }
}

void EditorHandleInput(Editor* editor) {
    if (ModalSystemHasActive(&editor->modal_system)) {
        Modal* top = GetTopModal(&editor->modal_system);
        Modal* after;
        ModalRepeatableFunc pred = (top && top->is_repeatable) ? top->is_repeatable : DefaultRawKeyIsRepeatable;
        RawInput input = InputSystemPollRawInput(&editor->input_system, pred);

        while (input.key != 0) {
            top = GetTopModal(&editor->modal_system);
            pred = (top && top->is_repeatable) ? top->is_repeatable : DefaultRawKeyIsRepeatable;
            if (top && top->custom_input) {
                top->custom_input(top, input);
            }
            after = GetTopModal(&editor->modal_system);
            if (top != after) {
                InputSystemClearHeld(&editor->input_system);
                pred = (after && after->is_repeatable) ? after->is_repeatable : DefaultRawKeyIsRepeatable;
            }
            input = InputSystemPollRawInput(&editor->input_system, pred);
        }
        return;
    }

    Action action = InputSystemPoll(&editor->input_system);

    while (action.type != ACTION_NONE) {
        if (editor->input_system.current_mode == MODE_TEXT) {
            DispatchInputTextMode(editor, action);
        } else if (editor->input_system.current_mode == MODE_COMMAND) {
            DispatchInputCommandMode(editor, action);
        }
        ClearAction(&action);
        action = InputSystemPoll(&editor->input_system);
    }
}
