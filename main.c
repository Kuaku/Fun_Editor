#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN 
    #define NOGDI             
    #define NOUSER               
    #include <windows.h>
    #undef WIN32_LEAN_AND_MEAN
    #undef NOGDI
    #undef NOUSER
    
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

#ifndef _WIN32
    #define min(a, b) ((a) < (b) ? (a) : (b))
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define BREAK_DOWN_RECT(rect) rect.position.x, rect.position.y, rect.size.x, rect.size.y

#define INITIAL_TEXT_BUFFER_CAPACITY 10
#define INITIAL_ADD_BUFFER_CAPACITY 4096
#define INITIAL_UNDO_STACK_CAPACITY 4096
#define INITIAL_PIECE_BUFFER_CAPACITY 1024

typedef enum { ORIGINAL, ADD } BufferType;
typedef enum { TYPE_DIR, TYPE_FILE, TYPE_ERROR } FileType;
typedef enum { MODE_TEXT, MODE_COMMAND, MODE_COUNT } EditorMode;

typedef enum {
    ACTION_NONE = 0,
    ACTION_CURSOR_LEFT,
    ACTION_CURSOR_RIGHT,
    ACTION_CURSOR_UP,
    ACTION_CURSOR_DOWN,
    ACTION_CURSOR_WORD_LEFT,
    ACTION_CURSOR_WORD_RIGHT,

    ACTION_SELECT_LEFT,
    ACTION_SELECT_RIGHT,
    ACTION_SELECT_UP,
    ACTION_SELECT_DOWN,
    ACTION_SELECT_WORD_LEFT,
    ACTION_SELECT_WORD_RIGHT,
    ACTION_SELECT_ALL,

    ACTION_INSERT_CHAR,
    ACTION_INSERT_NEWLINE,
    ACTION_INSERT_TAB,
    ACTION_DELETE_FORWARD,
    ACTION_DELETE_BACKWARD,

    ACTION_COPY,
    ACTION_CUT,
    ACTION_PASTE,

    ACTION_UNDO,
    ACTION_REDO,

    ACTION_SEARCH,
    ACTION_QUIT,
    ACTION_CANCEL,
    ACTION_OPEN_COMMAND_PALETTE
} ActionType;


const char* ActionTypeToString(ActionType type) {
    switch(type) {
        case ACTION_NONE: return "ACTION_NONE";
        case ACTION_CURSOR_LEFT: return "ACTION_CURSOR_LEFT";
        case ACTION_CURSOR_RIGHT: return "ACTION_CURSOR_RIGHT";
        case ACTION_CURSOR_UP: return "ACTION_CURSOR_UP";
        case ACTION_CURSOR_DOWN: return "ACTION_CURSOR_DOWN";
        case ACTION_CURSOR_WORD_LEFT: return "ACTION_CURSOR_WORD_LEFT";
        case ACTION_CURSOR_WORD_RIGHT: return "ACTION_CURSOR_WORD_RIGHT";
        case ACTION_SELECT_LEFT: return "ACTION_SELECT_LEFT";
        case ACTION_SELECT_RIGHT: return "ACTION_SELECT_RIGHT";
        case ACTION_SELECT_UP: return "ACTION_SELECT_UP";
        case ACTION_SELECT_DOWN: return "ACTION_SELECT_DOWN";
        case ACTION_SELECT_WORD_LEFT: return "ACTION_SELECT_WORD_LEFT";
        case ACTION_SELECT_WORD_RIGHT: return "ACTION_SELECT_WORD_RIGHT";
        case ACTION_SELECT_ALL: return "ACTION_SELECT_ALL";
        case ACTION_INSERT_CHAR: return "ACTION_INSERT_CHAR";
        case ACTION_INSERT_NEWLINE: return "ACTION_INSERT_NEWLINE";
        case ACTION_INSERT_TAB: return "ACTION_INSERT_TAB";
        case ACTION_DELETE_FORWARD: return "ACTION_DELETE_FORWARD";
        case ACTION_DELETE_BACKWARD: return "ACTION_DELETE_BACKWARD";
        case ACTION_COPY: return "ACTION_COPY";
        case ACTION_CUT: return "ACTION_CUT";
        case ACTION_PASTE: return "ACTION_PASTE";
        case ACTION_UNDO: return "ACTION_UNDO";
        case ACTION_REDO: return "ACTION_REDO";
        case ACTION_SEARCH: return "ACTION_SEARCH";
        case ACTION_QUIT: return "ACTION_QUIT";
        case ACTION_CANCEL: return "ACTION_CANCEL";
        case ACTION_OPEN_COMMAND_PALETTE: return "ACTION_OPEN_COMMAND_PALETTE";
        default: return "UNKNOWN_ACTION";
    }
}

typedef struct {
    ActionType type;
    char* text_buffer;
    size_t length;
} Action;

void ClearAction(Action* action) {
    if (action->text_buffer) {
        free(action->text_buffer);
    }
    action->length = 0;
}

typedef enum {
    MODI_NONE  = 0,
    MODI_CTRL  = 1 << 0,
    MODI_SHIFT = 1 << 1,
    MODI_ALT   = 1 << 2,
    MODI_SUPER = 1 << 3,
} ModifierFlags;

typedef struct {
    int key;
    ModifierFlags mods;
    ActionType action;
} KeyBinding;

static KeyBinding default_normal_bindings[] = {
    // Basic movement
    { KEY_LEFT,  MODI_NONE,  ACTION_CURSOR_LEFT },
    { KEY_RIGHT, MODI_NONE,  ACTION_CURSOR_RIGHT },
    { KEY_UP,    MODI_NONE,  ACTION_CURSOR_UP },
    { KEY_DOWN,  MODI_NONE,  ACTION_CURSOR_DOWN },
    
    // Word movement
    { KEY_LEFT,  MODI_CTRL,  ACTION_CURSOR_WORD_LEFT },
    { KEY_RIGHT, MODI_CTRL,  ACTION_CURSOR_WORD_RIGHT },
    
    // Selection variants (same keys + shift)
    { KEY_LEFT,  MODI_SHIFT, ACTION_SELECT_LEFT },
    { KEY_RIGHT, MODI_SHIFT, ACTION_SELECT_RIGHT },
    { KEY_UP,    MODI_SHIFT, ACTION_SELECT_UP },
    { KEY_DOWN,  MODI_SHIFT, ACTION_SELECT_DOWN },
    { KEY_LEFT,  MODI_CTRL | MODI_SHIFT, ACTION_SELECT_WORD_LEFT },
    { KEY_RIGHT, MODI_CTRL | MODI_SHIFT, ACTION_SELECT_WORD_RIGHT },
    { KEY_A,     MODI_CTRL,  ACTION_SELECT_ALL },
    
    // Editing
    { KEY_BACKSPACE, MODI_NONE, ACTION_DELETE_BACKWARD },
    { KEY_DELETE,    MODI_NONE, ACTION_DELETE_FORWARD },
    { KEY_ENTER,     MODI_NONE, ACTION_INSERT_NEWLINE },
    { KEY_TAB,       MODI_NONE, ACTION_INSERT_TAB },
    
    // Clipboard
    { KEY_C, MODI_CTRL, ACTION_COPY },
    { KEY_X, MODI_CTRL, ACTION_CUT },
    { KEY_V, MODI_CTRL, ACTION_PASTE },
    
    // History
    { KEY_Z, MODI_CTRL, ACTION_UNDO },
    { KEY_Y, MODI_CTRL, ACTION_REDO },
    { KEY_Z, MODI_CTRL | MODI_SHIFT, ACTION_REDO },  // Alternative
    
    // Editor
    { KEY_ESCAPE, MODI_NONE, ACTION_CANCEL },
    { KEY_Q,      MODI_CTRL, ACTION_QUIT },
};

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
    EditorMode current_mode;
    
    KeyBinding* bindings[MODE_COUNT];
    size_t binding_counts[MODE_COUNT];
    
} InputSystem;

InputSystem InitInputSystem() {
    InputSystem sys = {0};
    sys.current_mode = MODE_TEXT;

    sys.binding_counts[MODE_TEXT] = ARRAY_LEN(default_normal_bindings);
    sys.bindings[MODE_TEXT] = malloc(sizeof(default_normal_bindings));
    memcpy(sys.bindings[MODE_TEXT], default_normal_bindings, sizeof(default_normal_bindings));
    return sys;
}

ModifierFlags GetCurrentModifiers() {
    ModifierFlags mods = MODI_NONE;

    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        mods |= MODI_CTRL;
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        mods |= MODI_SHIFT;
    }
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
        mods |= MODI_ALT;
    }
    if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) {
        mods |= MODI_SUPER;
    }

    return mods;
}

ActionType LookupBinding(InputSystem* sys, int key, ModifierFlags mods) {
    KeyBinding* bindings = sys->bindings[sys->current_mode];
    size_t count = sys->binding_counts[sys->current_mode];

    for (size_t i = 0; i < count; i++) {
        if (bindings[i].key == key && bindings[i].mods == mods) {
            return bindings[i].action;
        }
    }

    return ACTION_NONE;
}

Action InputSystemPoll(InputSystem* sys) {
    Action action = { .type = ACTION_NONE };
    ModifierFlags mods = GetCurrentModifiers();

    if (!(mods & (MODI_CTRL | MODI_ALT | MODI_SUPER))) {
        int ch = GetCharPressed();
        if (ch != 0) {
            action.type = ACTION_INSERT_CHAR;
            action.text_buffer = malloc(1);
            action.text_buffer[0] = (char)ch;
            action.length = 1;
            return action;
        }
    }

    int key = GetKeyPressed();
    if (key != 0) {
        ActionType found = LookupBinding(sys, key, mods);
        action.type = found;
        return action;
    }

    return action;
}

void normalize_line_endings(char* buf) {
    char* src = buf;
    char* dst = buf;
    while (*src) {
        if (*src != '\r') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

FileType GetFileTypeFromPath(char* path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return TYPE_ERROR;
    } else if (S_ISDIR(path_stat.st_mode)) {
        return TYPE_DIR;
    } else if (S_ISREG(path_stat.st_mode)) {
        return TYPE_FILE;
    }

    return TYPE_ERROR;
}

char* LoadFile(const char* filename, size_t* out_len) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    rewind(f);

    char* buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "Out of memory!\n");
        return NULL;
    }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

typedef enum {
    EDIT_INSERT,
    EDIT_DELETE
} EditType;

typedef struct {
    EditType type;
    size_t position;
    size_t length;
    char* text;
    size_t cursor_before;
    size_t cursor_after;
} EditEntry;

void ClearEditEntry(EditEntry* entry) {
    if (!entry) return;
    
    if (entry->text) {
        free(entry->text);
        entry->text = NULL;
        entry->length = 0;
    }
}

typedef struct {
    EditEntry* entries;
    size_t count;
    size_t capacity;
    size_t current;
} UndoStack;

UndoStack InitUndoStack()  {
    UndoStack stack;
    stack.entries = calloc(INITIAL_UNDO_STACK_CAPACITY, sizeof(EditEntry));
    stack.capacity = INITIAL_UNDO_STACK_CAPACITY;
    stack.count = 0;
    stack.current = 0;
    return stack;
}

void ClearUndoStack(UndoStack* stack) {
    if (!stack) return;

    if (stack->entries) {
        for (size_t i = 0; i < stack->count; i++) {
            ClearEditEntry(&stack->entries[i]);
        }
        free(stack->entries);
    }
    stack->capacity = 0;
    stack->current = 0;
    stack->count = 0;
}

typedef struct {
    size_t x;
    size_t y;
} Position;

Vector2 PositionToVector(Position position) {
    return (Vector2){position.x, position.y};
}

typedef struct {
    Position position;
    Position size;
} Rect;

typedef struct {
    BufferType source;
    size_t start;
    size_t length;
} Piece;

typedef struct {
    Position* line_positions;
    size_t line_count;
    size_t capacity;
    bool is_valid;
} LineCache;

LineCache InitLineCache() {
    LineCache cache;
    cache.capacity = 1024;
    cache.line_positions = calloc(cache.capacity, sizeof(Position));
    cache.line_count = 0;
    cache.is_valid = false;
    return cache;
}
void ClearLineCache(LineCache* cache) {
    if (!cache) return;
    
    if (cache->line_positions) {
        free(cache->line_positions);
        cache->line_positions = NULL;
    }
    cache->line_count = 0;
    cache->capacity = 0;
    cache->is_valid = false;
}

typedef struct {
    char* file_path;

    char* org_buffer;
    size_t org_buffer_size;

    char* add_buffer;
    size_t add_buffer_capacity;
    size_t add_buffer_count;

    Piece* pieces;
    size_t piece_capacity;
    size_t piece_count;

    LineCache line_cache;

    size_t line_anchor;
    size_t offset_x;

    size_t pointer_position;
    size_t selection_start;
    size_t selection_end;

    Position pointer_position_cache;
    size_t last_pointer_position_cached;
    bool request_revalidate_pointer_cache;
    bool has_selection;

    UndoStack undo_stack;
} TextBuffer;

size_t GetTextSize(TextBuffer* buffer) {
    size_t out = 0;
    for (size_t i = 0; i < buffer->piece_count; ++i) {
        out += buffer->pieces[i].length;
    }
    return out;
}

void RebuildLineCache(TextBuffer* buffer) {
    buffer->line_cache.line_count = 0;
    buffer->line_cache.line_positions[0].x = 0;
    buffer->line_cache.line_positions[0].y = 0;
    size_t current_pos = 0;
    char* work_buffer;

    for (size_t i = 0; i < buffer->piece_count; ++i) {
        work_buffer = buffer->pieces[i].source == ORIGINAL ? buffer->org_buffer : buffer->add_buffer;

        for (size_t j = 0; j < buffer->pieces[i].length; ++j) {
            if (work_buffer[buffer->pieces[i].start + j] == '\n') {
                
                if (buffer->line_cache.line_count + 1 >= buffer->line_cache.capacity) {
                    buffer->line_cache.capacity *= 2;
                    buffer->line_cache.line_positions = realloc(buffer->line_cache.line_positions, buffer->line_cache.capacity * sizeof(Position));
                }

                buffer->line_cache.line_positions[buffer->line_cache.line_count].y = current_pos - buffer->line_cache.line_positions[buffer->line_cache.line_count].x;
                buffer->line_cache.line_count++;
                buffer->line_cache.line_positions[buffer->line_cache.line_count].x = current_pos + 1;
                buffer->line_cache.line_positions[buffer->line_cache.line_count].y = 0;
            }
            current_pos++;
        }
    }
    buffer->line_cache.line_positions[buffer->line_cache.line_count].y = current_pos - buffer->line_cache.line_positions[buffer->line_cache.line_count].x;
    buffer->line_cache.line_count++;
    buffer->line_cache.is_valid = true;
}

void InitTextBuffer(TextBuffer* buffer) {
    buffer->add_buffer = calloc(INITIAL_ADD_BUFFER_CAPACITY, sizeof(char));
    buffer->add_buffer_capacity = INITIAL_ADD_BUFFER_CAPACITY;    
    buffer->add_buffer_count = 0;

    buffer->line_cache = InitLineCache();
    
    buffer->line_anchor = 0;
    buffer->offset_x = 0;
    buffer->pointer_position = 0;
    buffer->pointer_position_cache = (Position){0, 0};
    buffer->last_pointer_position_cached = 0;
    buffer->request_revalidate_pointer_cache = false;

    buffer->selection_start = 0;
    buffer->selection_end = 0;
    buffer->has_selection = false;

    buffer->undo_stack = InitUndoStack();
}

void InitPieceBuffer(TextBuffer* buffer) {
    buffer->pieces = calloc(INITIAL_PIECE_BUFFER_CAPACITY, sizeof(Piece));
    buffer->piece_capacity = INITIAL_PIECE_BUFFER_CAPACITY;
    buffer->pieces[0].source = ORIGINAL;
    buffer->pieces[0].start = 0;
    buffer->pieces[0].length = strlen(buffer->org_buffer);
    
    buffer->piece_count = 1;
}

void InitEmptyTextBuffer(TextBuffer* buffer) {
    InitTextBuffer(buffer);

    buffer->file_path = NULL;
    buffer->org_buffer = strdup("");
    buffer->org_buffer_size = 0;

    InitPieceBuffer(buffer);
    RebuildLineCache(buffer);
}

void InitTextBufferFromPath(TextBuffer* buffer, const char* path) {
    InitTextBuffer(buffer);
    
    buffer->file_path = strdup(path);
    buffer->org_buffer = LoadFile(path, &buffer->org_buffer_size);
    normalize_line_endings(buffer->org_buffer);

    InitPieceBuffer(buffer);
    RebuildLineCache(buffer);
}

Position GetLinePosition(TextBuffer* buffer, size_t index) {
    if (!buffer->line_cache.is_valid) {
        RebuildLineCache(buffer);
    }

    return buffer->line_cache.line_positions[index];
}

Position GetLineByIndex(TextBuffer* buffer, size_t index) {
    return GetLinePosition(buffer, index);
}

size_t GetLineCount(TextBuffer* buffer)  {
    if (!buffer->line_cache.is_valid) {
        RebuildLineCache(buffer);
    }
    return buffer->line_cache.line_count;
}

char* GenerateLine(TextBuffer* buffer, size_t index) {
    //TODO: Use line cache
    char* line;
    Position line_position = GetLineByIndex(buffer, index);
    line = calloc(line_position.y + 1, sizeof(char));

    size_t start_pos = line_position.x;
    size_t end_pos = start_pos + line_position.y;

    size_t traversed = 0;
    char* temp = line;
    Piece piece;
    char* work_buffer;
    size_t copied = 0;
    for (size_t i = 0; i < buffer->piece_count && copied < line_position.y; ++i) {
        piece = buffer->pieces[i];
        work_buffer = piece.source == ORIGINAL ? buffer->org_buffer : buffer->add_buffer;
        
        size_t piece_start = traversed;
        size_t piece_end = traversed + piece.length;

        if (piece_end > start_pos && piece_start < end_pos) {
            size_t copy_start = (piece_start >= start_pos) ? 0 : start_pos - piece_start;
            size_t copy_end = (piece_end <= end_pos) ? piece.length : end_pos - piece_start;

            memcpy(line + copied, work_buffer + piece.start + copy_start, copy_end - copy_start);
            copied += copy_end - copy_start;
        }
        traversed += piece.length;
    }
    line[line_position.y] = '\0';
    return line;
}

Position IndexToPosition(TextBuffer* buffer, size_t index) {  
    //TODO: Use line cache
    Position out = {0, 0};
    size_t traversed = 0;
    char* work_buffer;
    for (size_t i = 0; i < buffer->piece_count && traversed < index; ++i) {
        work_buffer = buffer->pieces[i].source == ORIGINAL ? buffer->org_buffer : buffer->add_buffer;
        
        size_t to_read = index - traversed;
        if (to_read > buffer->pieces[i].length) to_read = buffer->pieces[i].length;
        for (size_t j = 0; j < to_read; ++j) {
            if (work_buffer[buffer->pieces[i].start + j] == '\n') {
                out.y++;
                out.x = 0;
            } else {
                out.x++;
            }
        }
        traversed += to_read;
    }    
    return out;
}

Position GetPointerPosition(TextBuffer* buffer) {
    if (!buffer->request_revalidate_pointer_cache && buffer->pointer_position == buffer->last_pointer_position_cached) return buffer->pointer_position_cache;
    Position out = IndexToPosition(buffer, buffer->pointer_position);

    buffer->request_revalidate_pointer_cache = false;
    buffer->pointer_position_cache = out;
    buffer->last_pointer_position_cached = buffer->pointer_position;
    return out;
}

void ClearTextBuffer(TextBuffer* buffer) {
    if (!buffer) return;

    if (buffer->file_path) {
        free(buffer->file_path);
        buffer->file_path = NULL;
    }

    if (buffer->org_buffer) {
        free(buffer->org_buffer);
        buffer->org_buffer = NULL;
    }
    buffer->org_buffer_size = 0;

    if (buffer->add_buffer) {
        free(buffer->add_buffer);
        buffer->add_buffer = NULL;
    }
    buffer->add_buffer_capacity = 0;
    buffer->add_buffer_count = 0;

    if (buffer->pieces) {
        free(buffer->pieces);
        buffer->pieces = NULL;
    }
    buffer->piece_capacity = 0;
    buffer->piece_count = 0;

    ClearLineCache(&buffer->line_cache);

    buffer->line_anchor = 0;
    buffer->pointer_position = 0;
    buffer->selection_start = 0;
    buffer->selection_end = 0;
    buffer->has_selection = false;

    ClearUndoStack(&buffer->undo_stack);
}

typedef struct {
    char* root_dir;

    TextBuffer* text_buffers;
    size_t text_buffers_capacity;
    size_t text_buffers_count;

    int open_text_buffer_index;
    bool exit_requested;

} EditorState;

EditorState InitEditorState(size_t capacity) {
    EditorState state;
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

    if (!state->text_buffers) {
        // TODO: Handle realloc fail gracefully
    }
}

size_t GetFreeTextBufferIndex(EditorState* state) {
    size_t index = state->text_buffers_count++;
    if (index >= state->text_buffers_capacity) {
        ResizeTextBuffers(state);
    }
    return index;
}

void OpenDirectoryFromPath(EditorState* state, const char* path) {
    state->root_dir = strdup(path);

    //TODO: introduce file cache and modal system
}

void OpenFileFromPath(EditorState* state, const char* path) {
    size_t index = GetFreeTextBufferIndex(state); 

    InitTextBufferFromPath(&state->text_buffers[index], path);
    state->open_text_buffer_index = index;
}
 
void OpenEmptyBuffer(EditorState* state) {
    size_t index = GetFreeTextBufferIndex(state); 

    InitEmptyTextBuffer(&state->text_buffers[index]);
    state->open_text_buffer_index = index;
}

typedef struct {
    Color background_color;
    Color mode_color;
    Color text_color;
    Color line_number_color;
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
} EditorSettings;

void ClearEditorSettings(EditorSettings* settings) {
    if (!settings) return;

    if (settings->editor_font.texture.id > 0) {
        UnloadFont(settings->editor_font);
    }
}

typedef struct {
    EditorState state;
    EditorSettings settings;
    InputSystem input_system;
} Editor;

Editor CreateEditor(EditorSettings settings, char* path) {
    Editor editor;
    editor.settings = settings;
    editor.state = InitEditorState(INITIAL_TEXT_BUFFER_CAPACITY);
    
    FileType root_type = TYPE_ERROR;
    if (path) {
        root_type = GetFileTypeFromPath(path);
    } 

    if (root_type == TYPE_FILE) {
        OpenFileFromPath(&editor.state, path);
    } else if (root_type == TYPE_DIR) {
        OpenDirectoryFromPath(&editor.state, path);
    } else {
        OpenEmptyBuffer(&editor.state);
    }

    editor.input_system = InitInputSystem();

    return editor;
}

void ClearEditor(Editor* editor) {
    if (!editor) return;

    ClearEditorState(&editor->state);
    ClearEditorSettings(&editor->settings);
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

void MovePointerLeft(TextBuffer* buffer) {
    if (buffer->pointer_position > 0) {
        buffer->pointer_position--;
        buffer->request_revalidate_pointer_cache = true; // TODO: strictly not needed!
    }
}

void MovePointerRight(TextBuffer* buffer) {
    if (buffer->pointer_position <= GetTextSize(buffer)) {
        buffer->pointer_position++;
        buffer->request_revalidate_pointer_cache = true; // TODO: strictly not needed!
    }
}

void MovePointerUp(TextBuffer* buffer) {
    Position pointer = GetPointerPosition(buffer);
    if (pointer.y == 0) {
        return;
    }
    Position nextLine = GetLineByIndex(buffer, pointer.y - 1);
    if (buffer->pointer_position != nextLine.x + min(nextLine.y, pointer.x)) {
        buffer->pointer_position = nextLine.x + min(nextLine.y, pointer.x);
        buffer->request_revalidate_pointer_cache = true; // TODO: strictly not needed!
    }   
}

void MovePointerDown(TextBuffer* buffer) {
    Position pointer = GetPointerPosition(buffer);
    size_t max_lines = GetLineCount(buffer);
    if (pointer.y >= max_lines - 1) {
        return;
    }
    Position nextLine = GetLineByIndex(buffer, pointer.y + 1);
    if (buffer->pointer_position != nextLine.x + min(nextLine.y, pointer.x)) {
        buffer->pointer_position = nextLine.x + min(nextLine.y, pointer.x);
        buffer->request_revalidate_pointer_cache = true; // TODO: strictly not needed!
    }
}

void MovePointerAction(Editor* editor, void(*move_function)(TextBuffer* buffer)) {
    TextBuffer* buffer = &editor->state.text_buffers[editor->state.open_text_buffer_index];
    size_t pointer_before = buffer->pointer_position;
    move_function(buffer);
    if (buffer->pointer_position != pointer_before) {
        buffer->has_selection = false;
    }
}
void MovePointerSelectionAction(Editor* editor, void(*move_function)(TextBuffer* buffer)) {
    TextBuffer* buffer = &editor->state.text_buffers[editor->state.open_text_buffer_index];
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

void DispatchInput(Editor* editor, Action action){
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
    default:
        TraceLog(LOG_INFO, "ActionType: %s is not implemented", ActionTypeToString(action.type));
    }
}

void EditorHandleInput(Editor* editor) {
    Action action = InputSystemPoll(&editor->input_system);

    while (action.type != ACTION_NONE) {
        DispatchInput(editor, action);
        ClearAction(&action);
        action = InputSystemPoll(&editor->input_system);
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
}

void SetupWindow() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 700, "Fun Editor");
    MaximizeWindow();
    
    SetTargetFPS(60); 
    SetExitKey(KEY_NULL);
}

int main(int argc, char** argv) {
    SetupWindow();

    ColorScheme scheme = {
        .background_color = (Color){32, 35, 41, 255},
        .mode_color = WHITE,
        .text_color = WHITE,
        .line_number_color = YELLOW
    };

    EditorSettings settings = {
        .scheme = scheme,
        .font_size = 30,
        .number_padding = 10,
        .pointer_padding = (Position){3, 3},
        .mode_padding = (Position){10, 10},
        .command_padding = (Position){10, 10},
        .pointer_width = 2,
        .editor_font = LoadFontEx("Input.ttf", 30, NULL, 0),
    };

    char* path = NULL;
    if (argc >= 2) {
        path = strdup(argv[1]);
    }

    Editor editor = CreateEditor(settings, path);
    if (path) {
        free(path);
        path = NULL;
    }

    while (!WindowShouldClose() && !ShouldEditorClose(&editor)) {
        BeginDrawing();

        EditorHandleInput(&editor);
        EditorRender(&editor);
        
        EndDrawing();
    }

    ClearEditor(&editor);
    return 0;
}