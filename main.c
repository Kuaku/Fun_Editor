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
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define INITIAL_TEXT_BUFFER_CAPACITY 10
#define INITIAL_ADD_BUFFER_CAPACITY 4096
#define INITIAL_UNDO_STACK_CAPACITY 4096
#define INITIAL_PIECE_BUFFER_CAPACITY 1024
#define INITIAL_COMMAND_BUFFER_CAPACITY 1024
#define INITIAL_COMMAND_BINDING_CAPACITY 1024
#define INITIAL_TOKENIZER_BUFFER_CAPACITY 8
#define INITIAL_MODAL_BUFFER_CAPACITY 8
#define INITIAL_MODAL_CACHE_BUFFER_CAPACITY 8

typedef struct Editor Editor;

typedef enum { ORIGINAL, ADD } BufferType;
typedef enum { TYPE_DIR, TYPE_FILE, TYPE_ERROR } FileType;
typedef enum { MODE_TEXT, MODE_COMMAND, MODE_COUNT } EditorMode;

typedef struct {
    size_t x;
    size_t y;
} Position;

typedef struct {
    Position position;
    Position size;
} Rect;

Vector2 PositionToVector(Position position) {
    return (Vector2){position.x, position.y};
}

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

    ACTION_GOTO,
    ACTION_SEARCH,
    ACTION_QUIT,
    ACTION_CANCEL,
    ACTION_OPEN_COMMAND_PALETTE,

    ACTION_OPEN_BUFFER_LIST,
    ACTION_OPEN_FILE,

    ACTION_EXECUTE_COMMAND
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
        case ACTION_GOTO: return "ACTION_GOTO";
        case ACTION_SEARCH: return "ACTION_SEARCH";
        case ACTION_QUIT: return "ACTION_QUIT";
        case ACTION_CANCEL: return "ACTION_CANCEL";
        case ACTION_OPEN_COMMAND_PALETTE: return "ACTION_OPEN_COMMAND_PALETTE";
        case ACTION_EXECUTE_COMMAND: return "ACTION_EXECUTE_COMMAND";
        case ACTION_OPEN_BUFFER_LIST: return "ACTION_OPEN_BUFFER_LIST";
        case ACTION_OPEN_FILE: return "ACTION_OPEN_FILE";
        default: return "UNKNOWN_ACTION";
    }
}

bool is_number(const char* str) {
    if (!str || *str == '\0') return false;
    
    const char* p = str;
    if (*p == '-') p++;  // allow negative
    
    if (*p == '\0') return false;  // just a minus sign
    
    while (*p) {
        if (!isdigit(*p)) return false;
        p++;
    }
    return true;
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

    // Command
    { KEY_P, MODI_CTRL, ACTION_OPEN_COMMAND_PALETTE},
    { KEY_G, MODI_CTRL, ACTION_GOTO},
    { KEY_F, MODI_CTRL, ACTION_SEARCH},
    { KEY_Q, MODI_CTRL, ACTION_QUIT },
    { KEY_B, MODI_CTRL, ACTION_OPEN_BUFFER_LIST },
    { KEY_O, MODI_CTRL, ACTION_OPEN_FILE }
};

static KeyBinding default_command_bindings[] = {
    { KEY_LEFT,  MODI_NONE,  ACTION_CURSOR_LEFT },
    { KEY_RIGHT, MODI_NONE,  ACTION_CURSOR_RIGHT },

    { KEY_BACKSPACE, MODI_NONE, ACTION_DELETE_BACKWARD },

    { KEY_P, MODI_CTRL, ACTION_OPEN_COMMAND_PALETTE},

    { KEY_ESCAPE, MODI_NONE, ACTION_CANCEL },

    { KEY_ENTER,     MODI_NONE, ACTION_EXECUTE_COMMAND },
};

typedef struct Modal Modal;

typedef void (*LayoutFunc)(Modal* modal);

typedef struct {
    Color background;
    Color border;
    Color text;
    Color text_muted;
    Color selection;
    Color input_background;
    Color focused_border;
    int border_width;
    Position content_padding;
    int widget_spacing;
    int title_height;
    Position title_padding;
    bool draw_title;
} ModalStyle;

typedef struct {
    ModifierFlags modifiers;
    int key;
} RawInput;

typedef void (*ModalRenderFunc)(Modal* modal, Rect content_bounds);
typedef void (*ModalInputFunc)(Modal* modal, RawInput input);
typedef void (*ModalCleanupFunc)(void* state);
typedef void (*ModalResultCallback)(Modal* modal, bool confirmed, void* result, void* user_data);

struct Modal {
    char* title;

    Rect bounds;
    Position wanted_size;
    Position min_size;
    Position max_size;
    Position margin; 
    
    ModalStyle style;

    LayoutFunc* layouts;
    size_t layout_count;
    size_t layout_capacity;

    bool is_cached;

    ModalRenderFunc custom_render;
    ModalInputFunc custom_input;
    ModalCleanupFunc cleanup;

    ModalResultCallback on_result;
    void* on_result_user_data;
    void* result_data;

    void* state;
};

void ApplyWantedSize(Modal* modal) {
    modal->bounds.size = modal->wanted_size;
}

void ApplyMaxSize(Modal* modal) {
    if (modal->max_size.x > 0 && modal->bounds.size.x > modal->max_size.x) {
        modal->bounds.size.x = modal->max_size.x;
    }
    if (modal->max_size.y > 0 && modal->bounds.size.y > modal->max_size.y) {
        modal->bounds.size.y = modal->max_size.y;
    }
}

void ApplyMinSize(Modal* modal) {
    if (modal->min_size.x > 0 && modal->bounds.size.x < modal->min_size.x) {
        modal->bounds.size.x = modal->min_size.x;
    }
    if (modal->min_size.y > 0 && modal->bounds.size.y < modal->min_size.y) {
        modal->bounds.size.y = modal->min_size.y;
    }
}

void ApplyMargin(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    size_t max_w = sw - modal->margin.x * 2;
    size_t max_h = sh - modal->margin.y * 2;
    if (modal->bounds.size.x > max_w) {
        modal->bounds.size.x = max_w;
    }
    if (modal->bounds.size.y > max_h) {
        modal->bounds.size.y = max_h;
    }
}

void CenterModal(Modal* modal) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    modal->bounds.position.x = (sw - modal->bounds.size.x) / 2;
    modal->bounds.position.y = (sh - modal->bounds.size.y) / 2;
}

void ModalAddLayout(Modal* modal, LayoutFunc layout) {
    if (modal->layouts == NULL) {
        modal->layout_capacity = 4;
        modal->layouts = calloc(modal->layout_capacity, sizeof(LayoutFunc));
    }
    if (modal->layout_count >= modal->layout_capacity) {
        modal->layout_capacity *= 2;
        modal->layouts = realloc(modal->layouts, modal->layout_capacity * sizeof(LayoutFunc));
    }
    modal->layouts[modal->layout_count++] = layout;
}

typedef struct {
    char** modal_keys;
    Modal** modal_cache;
    size_t cache_count;
    size_t cache_capacity;

    Modal** stack;
    size_t stack_count;
    size_t stack_capacity;
    
    // Default style
    ModalStyle default_style;
    
    // Font for rendering
    Font font;
    int font_size;
    
    int screen_width;
    int screen_height;
} ModalSystem;

ModalSystem InitModalSystem() {
    ModalSystem system;
    system.stack_capacity = INITIAL_MODAL_BUFFER_CAPACITY;
    system.stack = calloc(system.stack_capacity, sizeof(Modal*));
    system.stack_count = 0;

    system.cache_capacity = INITIAL_MODAL_CACHE_BUFFER_CAPACITY;
    system.modal_cache = calloc(system.cache_capacity, sizeof(Modal*));
    system.modal_keys = calloc(system.cache_capacity, sizeof(char*));
    system.cache_count = 0;


    system.default_style = (ModalStyle){
        .background    = (Color){45, 48, 55, 240},
        .border        = (Color){80, 85, 95, 255},
        .text          = WHITE,
        .text_muted    = (Color){150, 150, 150, 255},
        .selection     = (Color){60, 100, 180, 255},
        .input_background = (Color){30, 32, 38, 255},
        .focused_border   = (Color){100, 140, 220, 255},
        .border_width     = 2,
        .content_padding  = (Position){12, 12},
        .widget_spacing   = 4,
        .title_height     = 32,
    };

    return system;
}

void RegisterModalToQuickCatch(ModalSystem* system, const char* key, Modal* modal) {
    if (system->cache_count >= system->cache_capacity) {
        system->cache_capacity *= 2;
        system->modal_cache = realloc(system->modal_cache, system->cache_capacity * sizeof(Modal*));
        system->modal_keys = realloc(system->modal_keys, system->cache_capacity * sizeof(char*));
    }
    size_t index = system->cache_count++;
    system->modal_cache[index] = modal;
    system->modal_keys[index] = strdup(key);
}

Modal* CreateModal(ModalSystem* system, const char* title, Position wanted_size, ModalRenderFunc render, ModalInputFunc input, ModalCleanupFunc cleanup, void* state) {
    Modal* modal = calloc(1, sizeof(Modal));
    modal->title = strdup(title);
    modal->wanted_size = wanted_size;
    modal->style = system->default_style;
    modal->custom_render = render;
    modal->custom_input = input;
    modal->cleanup = cleanup;
    modal->state = state;

    return modal;
}

void PushModal(ModalSystem* system, Modal* modal) {
    if (system->stack_count >= system->stack_capacity) {
        system->stack_capacity *= 2;
        system->stack = realloc(system->stack, system->stack_capacity * sizeof(Modal*));
    }
    system->stack[system->stack_count++] = modal;
}

void PushModalFromCache(ModalSystem* system, char* key) {
    for (int i = 0; i < system->cache_count; i++) {
        if (strcmp(key, system->modal_keys[i]) == 0) {
          PushModal(system, system->modal_cache[i]);  
        }
    }
}

Modal* GetTopModal(ModalSystem* system) {
    if (system->stack_count == 0) return NULL;
    return system->stack[system->stack_count - 1];
}

void ClearModal(Modal* modal);

void CloseModal(ModalSystem* system, bool confirmed) {
    if (system->stack_count == 0) return;

    Modal* modal = system->stack[--system->stack_count];

    if (modal->on_result) {
        modal->on_result(modal, confirmed, modal->result_data, modal->on_result_user_data);
    }

    if (modal->is_cached) return;

    if (modal->cleanup) {
        modal->cleanup(modal->state);
    }

    ClearModal(modal);
    free(modal);
}

void ClearModal(Modal* modal) {
    if (modal->title) {
        free(modal->title);
    }

    if (modal->layouts) {
        free(modal->layouts);
    }

    if (modal->cleanup) {
        modal->cleanup(modal->state);
    }
}

bool ModalSystemHasActive(ModalSystem* system) {
    return system->stack_count > 0;
}

void ClearModalSystem(ModalSystem* system) {
    system->stack_count = 0;

    for (size_t i = 0; i < system->cache_count; i++) {
        Modal* modal = system->modal_cache[i];
        if (modal->cleanup) {
            modal->cleanup(modal->state);
        }
        ClearModal(modal);
    }

    free(system->stack);
    free(system->modal_cache);
    free(system->modal_keys);
}

typedef enum  {
    TOKENTYPE_STRING,
    TOKENTYPE_NUMBER
} CommandTokenType;

typedef struct {
    CommandTokenType type;
    char* char_value;
    int numb_value;
} CommandToken;

void ClearCommandToken(CommandToken* token) {
    if (token->char_value) {
        free(token->char_value);
    }
    token->numb_value = 0;
}

typedef struct {
    CommandToken* tokens;
    size_t capacity;
    size_t count;
} TokenList;

TokenList Tokenize(const char* command_buffer) {
    TokenList result = { NULL, 0};
    result.tokens = calloc(INITIAL_TOKENIZER_BUFFER_CAPACITY, sizeof(CommandToken));
    result.capacity = INITIAL_TOKENIZER_BUFFER_CAPACITY;

    const char* p = command_buffer;

    while(*p) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;

        CommandToken token = {0};

        if (*p == '"') {
            p++;
            const char* start = p;

            while (*p && *p != '"') p++;

            size_t len = p - start;
            token.type = TOKENTYPE_STRING;
            token.char_value = malloc(len + 1);
            strncpy(token.char_value, start, len);
            token.char_value[len] = '\0';
            
            if (*p == '"') p++;
        } else {
            const char* start = p;

            while (*p && !isspace(*p)) p++;

            size_t len = p - start;
            char* value = malloc(len + 1);
            strncpy(value, start, len);
            value[len] = '\0';

            if (is_number(value)) {
                token.type = TOKENTYPE_NUMBER;
                token.numb_value = atoi(value);
                token.char_value = NULL;
                free(value);
            } else {
                token.type = TOKENTYPE_STRING;
                token.char_value = value;
            }
        }

        while (result.count >= result.capacity) {
            result.capacity *= 2;
            result.tokens = realloc(result.tokens, result.capacity * sizeof(CommandToken));
        }
        result.tokens[result.count++] = token;
    }

    return result;
}

void ClearTokenList(TokenList* list) {
    if (list->tokens) {
        for (int i = 0; i < list->count; i++) {
            ClearCommandToken(&list->tokens[i]);
        }
        free(list->tokens);
    }
    list->capacity = 0;
    list->count = 0;
}

typedef void (*CommandExecuteFunc)(Editor* editor, CommandToken* tokens, size_t token_count);

typedef struct {
    char* command;
    CommandTokenType* needed_types;
    size_t needed_types_count;

    CommandExecuteFunc execute;
} CommandBinding;


CommandBinding CreateCommandBinding(const char* command, CommandTokenType* needed_types, size_t needed_types_count, CommandExecuteFunc execute) {
    CommandBinding binding;

    binding.command = strdup(command);
    if (needed_types_count > 0 && needed_types) {
        binding.needed_types = calloc(needed_types_count, sizeof(CommandTokenType));
        memcpy(binding.needed_types, needed_types, needed_types_count * sizeof(CommandTokenType));
    } else {
        binding.needed_types = NULL;
    }
    binding.needed_types_count = needed_types_count;
    binding.execute = execute;

    return binding;
}

void ClearCommandBinding(CommandBinding* binding) {
    if (binding->command) {
        free(binding->command);
    }

    if (binding->needed_types) {
        free(binding->needed_types);
    }

    binding->needed_types_count = 0;
}

typedef struct {
    char* command_buffer;
    size_t command_buffer_capacity;

    CommandBinding* bindings;
    size_t command_bindings_capacity;
    size_t command_bindings_count;

    size_t pointer_position;
} CommandSystem;

CommandSystem InitCommandSystem() {
    CommandSystem system;
    system.command_buffer = calloc(INITIAL_COMMAND_BUFFER_CAPACITY, sizeof(char));
    system.command_buffer_capacity = INITIAL_COMMAND_BUFFER_CAPACITY;
    system.pointer_position = 0;

    system.bindings = calloc(INITIAL_COMMAND_BINDING_CAPACITY, sizeof(CommandBinding));
    system.command_bindings_capacity = INITIAL_COMMAND_BINDING_CAPACITY;
    system.command_bindings_count = 0;

    return system;
}

void CommandSystemInsertString(CommandSystem* system, char* value, size_t len) {
    size_t command_buffer_length = strlen(system->command_buffer);
    while (command_buffer_length + len >= system->command_buffer_capacity) {
        system->command_buffer = realloc(system->command_buffer, system->command_buffer_capacity * 2 * sizeof(char));
        system->command_buffer_capacity *= 2;
    }
    memmove(system->command_buffer + system->pointer_position + len, system->command_buffer + system->pointer_position, command_buffer_length - system->pointer_position + 1);
    memcpy(system->command_buffer + system->pointer_position, value, len);
    system->pointer_position += len;
}

void CommandSystemRemoveChar(CommandSystem* system) {
    size_t command_buffer_length = strlen(system->command_buffer);
    
    if (system->pointer_position >= command_buffer_length) {
        return;
    }
    
    memmove(system->command_buffer + system->pointer_position,
            system->command_buffer + system->pointer_position + 1,
            command_buffer_length - system->pointer_position);
}

void CommandSystemBackspace(CommandSystem* system) {
    if (system->pointer_position == 0) {
        return;
    }
    
    system->pointer_position--;
    CommandSystemRemoveChar(system);
}

void MoveCommandPointerLeft(CommandSystem* system) {
    if (system->pointer_position <= 0) return;
    system->pointer_position--;
}

void MoveCommandPointerRight(CommandSystem* system) {
    size_t command_buffer_length = strlen(system->command_buffer);
    if (system->pointer_position >= command_buffer_length) return;
    system->pointer_position++;
}

void AddCommandBinding(CommandSystem* system, CommandBinding binding) {
    while (system->command_bindings_count >= system->command_bindings_capacity) {
        system->bindings = realloc(system->bindings, system->command_bindings_capacity * 2 * sizeof(CommandBinding));
        system->command_bindings_capacity *= 2;
    }
    system->bindings[system->command_bindings_count++] = binding;
}

void ClearCommandSystem(CommandSystem* system) {
    if (system->command_buffer) {
        free(system->command_buffer);
    }

    if (system->bindings) {
        for (int i = 0; i < system->command_bindings_count; i++) {
            ClearCommandBinding(&system->bindings[i]);
        }
        free(system->bindings);
    }

    system->command_buffer_capacity = 0;
    system->pointer_position = 0;
}

typedef struct {
    EditorMode current_mode;
    
    KeyBinding* bindings[MODE_COUNT];
    size_t binding_counts[MODE_COUNT];
    
    CommandSystem command_system;
} InputSystem;

InputSystem InitInputSystem() {
    InputSystem sys = {0};
    sys.current_mode = MODE_TEXT;

    sys.binding_counts[MODE_TEXT] = ARRAY_LEN(default_normal_bindings);
    sys.bindings[MODE_TEXT] = malloc(sizeof(default_normal_bindings));
    memcpy(sys.bindings[MODE_TEXT], default_normal_bindings, sizeof(default_normal_bindings));

    sys.binding_counts[MODE_COMMAND] = ARRAY_LEN(default_command_bindings);
    sys.bindings[MODE_COMMAND] = malloc(sizeof(default_command_bindings));
    memcpy(sys.bindings[MODE_COMMAND], default_command_bindings, sizeof(default_command_bindings));
    
    sys.command_system = InitCommandSystem();
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
            action.text_buffer = malloc(2);
            action.text_buffer[0] = (char)ch;
            action.text_buffer[1] = '\0';
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

bool HasModifiers(ModifierFlags modiefers, ModifierFlags check) {
    return modiefers & check;
}

RawInput InputSystemPollRawInput() {
    RawInput input = {0};
    input.modifiers = GetCurrentModifiers();
    input.key = GetCharPressed();
    if (!HasModifiers(input.modifiers, MODI_CTRL | MODI_ALT | MODI_SUPER)) {
        int ch = GetCharPressed();
        if (ch != 0) {
            input.key = ch;
            return input;
        }
    }

    input.key = GetKeyPressed();
    return input;
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

void ClearInputSystem(InputSystem* system) {
    ClearCommandSystem(&system->command_system);   
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

    double time_since_last_edit;

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

void PushCommand(TextBuffer* buffer, EditType type, size_t position, const char* text, size_t length) {
    UndoStack* stack = &buffer->undo_stack;
    for (size_t i = stack->current; i < stack->count; i++) {
        ClearEditEntry(&stack->entries[i]);      
    }
    stack->count = stack->current;

    if (stack->count >= stack->capacity) {
        ClearEditEntry(&stack->entries[0]);
        memmove(stack->entries, stack->entries + 1, (stack->capacity - 1) * sizeof(EditEntry));
        stack->count--;
    }
    
    EditEntry* entry = &stack->entries[stack->count];
    entry->type = type;
    entry->position = position;
    entry->length = length;
    entry->cursor_before = buffer->pointer_position;
    switch (entry->type)
    {
        case EDIT_INSERT:
            entry->cursor_after = buffer->pointer_position + entry->length;
            break;
        case EDIT_DELETE:
            entry->cursor_after = buffer->pointer_position - entry->length;
            break;
    }

    if (text && length > 0) {
        entry->text = malloc(length + 1);
        memcpy(entry->text, text, length);
        entry->text[length] = '\0';
    } else {
        entry->text = NULL;
    }

    stack->count++;
    stack->current = stack->count;
}

char GetCharAt(TextBuffer* buffer, size_t position) {
    size_t traversed = 0;
    char* work_buffer;
    
    for (size_t i = 0; i < buffer->piece_count; i++) {
        work_buffer = buffer->pieces[i].source == ORIGINAL ? buffer->org_buffer : buffer->add_buffer;
        
        if (traversed + buffer->pieces[i].length > position) {
            size_t offset = position - traversed;
            return work_buffer[buffer->pieces[i].start + offset];
        }
        
        traversed += buffer->pieces[i].length;
    }
    
    return '\0'; 
}

bool TryToMergeCharacterRemove(TextBuffer* buffer, float current_time) {
    UndoStack* stack = &buffer->undo_stack;
    if (stack->current > 0 && current_time - buffer->time_since_last_edit < 1.0) {
        EditEntry* prev = &stack->entries[stack->current - 1];
        if (prev->type == EDIT_DELETE && prev->position == buffer->pointer_position) {
            char deleted = GetCharAt(buffer, buffer->pointer_position - 1);

            char* new_text = malloc(prev->length + 2);
            new_text[0] = deleted;
            memcpy(new_text + 1, prev->text, prev->length);
            new_text[prev->length + 1] = '\0';

            free(prev->text);
            prev->text = new_text;
            prev->length++;
            prev->position--;
            prev->cursor_after = buffer->pointer_position - 1;
            
            return true;
        }
    }
    return false;
}

bool TryToMergeCharacterInsert(TextBuffer* buffer, char* value, size_t len, float current_time) {
    UndoStack* stack = &buffer->undo_stack;
    if (stack->current > 0 && current_time - buffer->time_since_last_edit < 1.0) {
        EditEntry* prev = &stack->entries[stack->current - 1];
        if (prev->type == EDIT_INSERT && 
            prev->position + prev->length == buffer->pointer_position &&
            memchr(value, '\n', len) == NULL) {
            
            char* new_text = realloc(prev->text, prev->length + len + 1);
            if (!new_text) {
                return false;
            }
            memcpy(new_text + prev->length, value, len);
            new_text[prev->length + len] = '\0';
            prev->text = new_text;
            prev->length += len;
            prev->cursor_after = buffer->pointer_position + len;
            return true;
        }
    }
    return false;
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
                
                while (buffer->line_cache.line_count + 1 >= buffer->line_cache.capacity) {
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
    buffer->time_since_last_edit = 0;

    buffer->selection_start = 0;
    buffer->selection_end = 0;
    buffer->has_selection = false;

    buffer->undo_stack = InitUndoStack();
}

char* GetTextRange(TextBuffer* buffer, size_t start, size_t end) {
    size_t length = end - start;
    char* result = malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        result[i] = GetCharAt(buffer, start + i);
    }
    result[length] = '\0';
    return result;
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
    state->text_buffers_capacity = new_size;

    if (!state->text_buffers) {
        // TODO: Handle realloc fail gracefully
    }
}

size_t GetFreeTextBufferIndex(EditorState* state) {
    size_t index = state->text_buffers_count++;
    while (index >= state->text_buffers_capacity) {
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
    Color command_color;
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

struct Editor{
    EditorState state;
    EditorSettings settings;
    InputSystem input_system;
    ModalSystem modal_system;
};

void ModalSystemRender(Editor* editor) {
    ModalSystem* system = &editor->modal_system;
    if (system->stack_count == 0) return;
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 120});

    Modal* active_modal = GetTopModal(system);

    for (size_t i = 0; i < active_modal->layout_count; i++) {
        active_modal->layouts[i](active_modal);
    }

    Rect bounding = active_modal->bounds;

    BeginScissorMode(BREAK_DOWN_RECT(bounding));

    DrawRectangle(BREAK_DOWN_RECT(bounding), active_modal->style.background);

    if (active_modal->style.draw_title) {
        int title_height = active_modal->style.title_height + active_modal->style.title_padding.y * 2;

        DrawTextEx(editor->settings.editor_font, active_modal->title, (Vector2){bounding.position.x + active_modal->style.title_padding.x, bounding.position.y + active_modal->style.title_padding.y}, active_modal->style.title_height, 1, active_modal->style.text);


        bounding.position.y += title_height;
        bounding.size.y -= title_height;
    }


    if (active_modal->custom_render) {
        active_modal->custom_render(active_modal, bounding);
    }

    EndScissorMode();
}

typedef struct {
    Editor* editor;
    size_t selected_index;
    size_t scroll_offset;
} BufferListState;

void BufferListRender(Modal* modal, Rect content) {
    BufferListState* state = (BufferListState*)modal->state;
    Editor* editor = state->editor;
    EditorState* es = &editor->state;
    Font font = editor->settings.editor_font;
    int font_size = editor->settings.font_size;
    int row_height = font_size + modal->style.widget_spacing;
    size_t visible_rows = content.size.y / row_height;

    if (state->selected_index < state->scroll_offset) {
        state->scroll_offset = state->selected_index;
    }
    if (state->selected_index >= state->scroll_offset + visible_rows) {
        state->scroll_offset = state->selected_index - visible_rows + 1;
    }

    BeginScissorMode(content.position.x, content.position.y,
                     content.size.x, content.size.y);

    for (size_t i = state->scroll_offset; i < es->text_buffers_count; i++) {
        size_t display_i = i - state->scroll_offset;
        if (display_i >= visible_rows) break;

        int y = content.position.y + display_i * row_height;

        const char* full_path = es->text_buffers[i].file_path;
        const char* label = "[untitled]";
        if (full_path) {
            const char* slash = strrchr(full_path, '/');
            const char* bslash = strrchr(full_path, '\\');
            if (bslash && (!slash || bslash > slash)) slash = bslash;
            label = slash ? slash + 1 : full_path;
        }

        const char* prefix = (i == state->selected_index) ? "> " : "  ";
        Color text_color = modal->style.text;
        if ((int)i == es->open_text_buffer_index) {
            text_color = modal->style.focused_border;
        }

        DrawTextEx(font, TextFormat("%s%s", prefix, label),
                   (Vector2){content.position.x + 4,
                             y + modal->style.widget_spacing / 2},
                   font_size, 1, text_color);
    }

    EndScissorMode();
}

void BufferListInput(Modal* modal, RawInput input) {
    BufferListState* state = (BufferListState*)modal->state;
    size_t count = state->editor->state.text_buffers_count;

    switch (input.key) {
        case KEY_DOWN:
            if (state->selected_index + 1 < count) {
                state->selected_index++;
            }
            break;
        case KEY_UP:
            if (state->selected_index > 0) {
                state->selected_index--;
            }
            break;
        case KEY_ENTER:
            modal->result_data = (void*)(uintptr_t)state->selected_index;
            CloseModal(&state->editor->modal_system, true);
            break;
        case KEY_ESCAPE:
            CloseModal(&state->editor->modal_system, false);
            break;
    }
}

void BufferListResult(Modal* modal, bool confirmed, void* result, void* user_data) {
    if (!confirmed) return;

    Editor* editor = (Editor*)user_data;
    size_t index = (size_t)(uintptr_t)result;

    if (index < editor->state.text_buffers_count) {
        editor->state.open_text_buffer_index = (int)index;
    }
}

void BufferListCleanup(void* state) {
    free(state);
}

void RegisterBufferListModal(Editor* editor) {
    BufferListState* state = calloc(1, sizeof(BufferListState));
    state->editor = editor;
    state->selected_index = 0;
    state->scroll_offset = 0;

    Modal* modal = CreateModal(
        &editor->modal_system,
        "Open Buffers",
        (Position){500, 400},
        BufferListRender,
        BufferListInput,
        BufferListCleanup,
        state
    );
    modal->style.draw_title = true;
    modal->style.title_padding = (Position){10, 10};
    modal->on_result = BufferListResult;
    modal->on_result_user_data = editor;
    modal->is_cached = true;
    modal->margin = (Position){50, 50};

    ModalAddLayout(modal, ApplyWantedSize);
    ModalAddLayout(modal, ApplyMinSize);
    ModalAddLayout(modal, ApplyMaxSize);
    ModalAddLayout(modal, ApplyMargin);
    ModalAddLayout(modal, CenterModal);

    RegisterModalToQuickCatch(&editor->modal_system, "buffer_list", modal);
}

void OpenBufferListModal(Editor* editor) {
    PushModalFromCache(&editor->modal_system, "buffer_list");

    Modal* top = GetTopModal(&editor->modal_system);
    if (top) {
        BufferListState* state = (BufferListState*)top->state;
        state->selected_index = editor->state.open_text_buffer_index >= 0
                              ? (size_t)editor->state.open_text_buffer_index : 0;
        state->scroll_offset = 0;
    }
}

void TryExecuteCommandSystem(Editor* editor) {
    CommandSystem* system = &editor->input_system.command_system;
    TokenList tokens = Tokenize(system->command_buffer);
    
    if (tokens.count == 0 || tokens.tokens[0].type != TOKENTYPE_STRING) {
        ClearTokenList(&tokens);
        return;
    }

    for (size_t i = 0; i < system->command_bindings_count; i++) {
        CommandBinding* binding = &system->bindings[i];

        if (strcmp(tokens.tokens[0].char_value, binding->command) != 0) {
            continue;
        }

        if (tokens.count - 1 < binding->needed_types_count) {
            continue;
        }

        bool types_match = true;

        for (size_t j = 0; j < binding->needed_types_count; j++) {
            if (tokens.tokens[j + 1].type != binding->needed_types[j]) {
                types_match = false;
                break;
            }
        }

        if (types_match) {
            if (binding->execute) {
                binding->execute(editor, &tokens.tokens[1], tokens.count - 1);   
            }
            ClearTokenList(&tokens);
            return;
        }
    }
    ClearTokenList(&tokens);
}

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
    editor.modal_system = InitModalSystem();
    
    RegisterBufferListModal(&editor);

    return editor;
}

TextBuffer* GetActiveBuffer(Editor* editor) {
    return &editor->state.text_buffers[editor->state.open_text_buffer_index];
}

void ClearEditor(Editor* editor) {
    if (!editor) return;

    ClearEditorState(&editor->state);
    ClearEditorSettings(&editor->settings);
    ClearInputSystem(&editor->input_system);
    ClearModalSystem(&editor->modal_system);
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

bool IsWordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsPunct(char c) {
    return c && !IsWordChar(c) && c != ' ' && c != '\t' && c != '\n';
}

void MovePointerWordRight(TextBuffer* buffer) {
    size_t size = GetTextSize(buffer);
    if (buffer->pointer_position >= size) return;
    char c = GetCharAt(buffer, buffer->pointer_position);
    
    if (c == '\n') {
        buffer->pointer_position++;
        return;
    }
    
    if (IsWordChar(c)) {
        while (buffer->pointer_position < size && IsWordChar(GetCharAt(buffer, buffer->pointer_position))) buffer->pointer_position++;
    } else if (IsPunct(c)) {
        while (buffer->pointer_position < size && IsPunct(GetCharAt(buffer, buffer->pointer_position))) buffer->pointer_position++;
    } else {
        while (buffer->pointer_position < size && (c = GetCharAt(buffer, buffer->pointer_position), c == ' ' || c == '\t')) buffer->pointer_position++;
        if (buffer->pointer_position < size && GetCharAt(buffer, buffer->pointer_position) == '\n') return;
        while (buffer->pointer_position < size && IsPunct(GetCharAt(buffer, buffer->pointer_position))) buffer->pointer_position++;
    }
}

void MovePointerWordLeft(TextBuffer* buffer) {
    size_t size = GetTextSize(buffer);
    if (buffer->pointer_position == 0) return;
    buffer->pointer_position--;
    
    char c = GetCharAt(buffer, buffer->pointer_position);
    if (c == '\n') return;
    
    while (buffer->pointer_position > 0 && (c = GetCharAt(buffer, buffer->pointer_position), c == ' ' || c == '\t')) {
        if (GetCharAt(buffer, buffer->pointer_position - 1) == '\n') return;
        buffer->pointer_position--;
    }
    
    c = GetCharAt(buffer, buffer->pointer_position);
    if (IsWordChar(c)) {
        while (buffer->pointer_position > 0 && IsWordChar(GetCharAt(buffer, buffer->pointer_position - 1))) buffer->pointer_position--;
    } else if (IsPunct(c)) {
        while (buffer->pointer_position > 0 && IsPunct(GetCharAt(buffer, buffer->pointer_position - 1))) buffer->pointer_position--;
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

size_t AppendAddBuffer(TextBuffer* buffer, char* value, size_t len) {
    while (buffer->add_buffer_count + len >= buffer->add_buffer_capacity) {
        buffer->add_buffer = (char*)realloc(buffer->add_buffer, buffer->add_buffer_capacity * 2 * sizeof(char));
        buffer->add_buffer_capacity *= 2;
    }

    size_t index = buffer->add_buffer_count;
    
    memcpy(&buffer->add_buffer[buffer->add_buffer_count], value, len);

    buffer->add_buffer_count += len;
    
    return index;
}

void InsertString(TextBuffer* buffer, size_t position, char* value, size_t len) {
    // TODO: Use memove and inplace to make this function O(n) for the insertion!
    size_t new_start = AppendAddBuffer(buffer, value, len);

    Piece new_piece = {ADD, new_start, len};

    Piece* new_pieces = malloc((buffer->piece_count + 2) * sizeof(Piece));
    size_t new_count = 0; 
    size_t current_pos = 0;
    bool inserted = false;

    if (buffer->piece_count == 0) {
        new_pieces[new_count++] = new_piece;
        inserted = true;
    }

    for(size_t i = 0; i < buffer->piece_count; i++) {
        Piece p = buffer->pieces[i];
        if (current_pos + p.length < position) {
            new_pieces[new_count++] = p;
            current_pos += p.length;
        } else {
            size_t offset = position - current_pos;
            if (offset > 0) {
                Piece left = p;
                left.length = offset;
                new_pieces[new_count++] = left;
            }

            new_pieces[new_count++] = new_piece;
            inserted = true;

            if (offset < p.length) {
                Piece right = p;
                right.start += offset;
                right.length -= offset;
                new_pieces[new_count++] = right;
            }    

            for (size_t j = i + 1; j < buffer->piece_count; j++) {
                new_pieces[new_count++] = buffer->pieces[j];
            }
            break;
        }
    }

    if (!inserted) {
        new_pieces[new_count++] = new_piece;
    }

    while (new_count > buffer->piece_capacity) {
        buffer->piece_capacity = new_count * 2;
        buffer->pieces = realloc(buffer->pieces, buffer->piece_capacity * sizeof(Piece));
    }

    memcpy(buffer->pieces, new_pieces, sizeof(Piece) * new_count);
    buffer->piece_count = new_count;
    free(new_pieces);

    buffer->line_cache.is_valid = false;
}

bool RemoveCharacter(TextBuffer* buffer, size_t position) {
    if (position > 0) {
        Piece* new_pieces = malloc((buffer->piece_count + 1) * sizeof(Piece));
        int new_count = 0;
        size_t current_pos = 0;

        position--;

        for (size_t i = 0; i < buffer->piece_count; ++i) {
            Piece p = buffer->pieces[i];

            if (current_pos + p.length <= position) {
                new_pieces[new_count++] = p;
                current_pos += p.length;
            } else if (position >= current_pos && position < current_pos + p.length) {
                size_t local_offset = position - current_pos;

                if (local_offset > 0) {
                    Piece left = p;
                    left.length = local_offset;
                    new_pieces[new_count++] = left;
                }

                if (local_offset + 1 < p.length) {
                    Piece right = p;
                    right.start += local_offset + 1;
                    right.length -= local_offset + 1;
                    new_pieces[new_count++] = right;
                }

                current_pos += p.length;
            } else {
                new_pieces[new_count++] = p;
                current_pos += p.length;
            }
        }

        while (new_count > buffer->piece_capacity) {
            buffer->piece_capacity = new_count * 2;
            buffer->pieces = realloc(buffer->pieces, buffer->piece_capacity * sizeof(Piece));
        }
        memcpy(buffer->pieces, new_pieces, sizeof(Piece) * new_count);
        buffer->piece_count = new_count;
        buffer->line_cache.is_valid = false;
        return true;
    }
    return false;
}

void ExecuteDelete(TextBuffer* buffer, size_t position, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        RemoveCharacter(buffer, position + 1);
    }
    buffer->pointer_position = position;
}

void RemoveArea(TextBuffer* buffer, size_t position, size_t length) {
    char* deleted_text = GetTextRange(buffer, position, position + length);
    PushCommand(buffer, EDIT_DELETE, position, deleted_text, length);
    free(deleted_text);

    ExecuteDelete(buffer, position, length);
}

void RemoveSelection(TextBuffer* buffer) {
    size_t selection_length = abs((int)(buffer->selection_end) - (int)(buffer->selection_start));
    RemoveArea(buffer, min(buffer->selection_start, buffer->selection_end), selection_length);
    buffer->has_selection = false;
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

        if (!TryToMergeCharacterRemove(buffer, current_time)) {
            char deleted_char = GetCharAt(buffer, buffer->pointer_position - 1);
            PushCommand(buffer, EDIT_DELETE, buffer->pointer_position - 1, &deleted_char, 1);
        }

        if (RemoveCharacter(buffer, buffer->pointer_position)) {
            buffer->pointer_position--;
        }
        buffer->time_since_last_edit = current_time;
    }
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

void ToggleCommandModeAction(Editor* editor) {
    if (editor->input_system.current_mode == MODE_COMMAND) {
        editor->input_system.current_mode = MODE_TEXT;
    } else {
        editor->input_system.current_mode = MODE_COMMAND;
    }
}

void ResetCommandBuffer(CommandSystem* system) {
    memset(system->command_buffer, 0, system->command_buffer_capacity);
    system->pointer_position = 0;
}

void OpenCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    struct stat st;
    if (stat(tokens[0].char_value, &st) != 0 || !S_ISREG(st.st_mode)) {
        return;
    }
    OpenFileFromPath(&editor->state, tokens[0].char_value);
    ResetCommandBuffer(&editor->input_system.command_system);
    editor->input_system.current_mode = MODE_TEXT;
}

void QuitCommando(Editor* editor, CommandToken* tokens, size_t token_count) {
    editor->state.exit_requested = true;
}

void GotoCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    size_t line = tokens[0].numb_value - 1;
    if (line >= 0 && line < GetLineCount(buffer)) {
        buffer->pointer_position = GetLineByIndex(buffer, line).x;
        ResetCommandBuffer(&editor->input_system.command_system);
        editor->input_system.current_mode = MODE_TEXT;
    }
}

size_t PositionToIndex(TextBuffer* buffer, Position in) {
    if (!buffer->line_cache.is_valid) {
        RebuildLineCache(buffer);
    }
    size_t out = 0;

    for (size_t i = 0; i < in.y; ++i) {
        Position line = GetLineByIndex(buffer, i);
        out += line.y + 1;
    }

    out += in.x;

    return out;
}

void FindCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    size_t line_counter = GetPointerPosition(buffer).y + 1;
    size_t line_count = GetLineCount(buffer);
    size_t search_length = strlen(tokens[0].char_value);
    Position found = {-1, -1};
    for (size_t i = 0; i < line_count; ++i) {
        size_t working_line = (line_counter + i) % line_count;
        char* line = GenerateLine(buffer, working_line);
        size_t line_length = strlen(line);
        
        if (line_length < search_length) {
            continue;
        }

        for (size_t j = 0; j < line_length - search_length; ++j) {
            if (strncmp(line + j, tokens[0].char_value, search_length) == 0) {
                found.x = j;
                found.y = working_line;
                break;
            }
        }
        free(line);
        if (found.x != -1) {
            break;
        }
    }
    if (found.x != -1) {
        buffer->pointer_position = PositionToIndex(buffer, found) + search_length;
        buffer->has_selection = true;
        buffer->selection_start = buffer->pointer_position;
        buffer->selection_end = buffer->pointer_position - search_length;
    }
}

void RegisterDefaultCommandBinding(CommandSystem* system) {
    CommandTokenType goto_types[] = { TOKENTYPE_NUMBER };
    AddCommandBinding(system, CreateCommandBinding("goto", goto_types, 1, GotoCommand));

    CommandTokenType find_types[] = { TOKENTYPE_STRING };
    AddCommandBinding(system, CreateCommandBinding("find", find_types, 1, FindCommand));
    
    AddCommandBinding(system, CreateCommandBinding("quit", NULL, 0, QuitCommando));

    CommandTokenType open_types[] = { TOKENTYPE_STRING };
    AddCommandBinding(system, CreateCommandBinding("open", open_types, 1, OpenCommand));
}

void EnterCommandModeWithCommand(Editor* editor, const char* command, size_t pointer_position) {
    CommandSystem* system = &editor->input_system.command_system;
    size_t command_length = strlen(command);

    while (command_length >= system->command_buffer_capacity) {
        system->command_buffer = realloc(system->command_buffer, system->command_buffer_capacity * sizeof(char) * 2);
        system->command_buffer_capacity *= 2;
    }

    memset(system->command_buffer, 0, system->command_buffer_capacity);
    memcpy(system->command_buffer, command, command_length);
    system->pointer_position = pointer_position;

    editor->input_system.current_mode = MODE_COMMAND;
}

void DispatchInputTextMode(Editor* editor, Action action){
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

void EditorHandleInput(Editor* editor) {
    if (ModalSystemHasActive(&editor->modal_system)) {
        RawInput input = InputSystemPollRawInput();

        while (input.key != 0) {
            Modal* top = GetTopModal(&editor->modal_system);
            if (top && top->custom_input) {
                top->custom_input(top, input);
            }

            input = InputSystemPollRawInput();
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

void EditorRenderCommand(Editor* editor) {
    int screen_height = GetScreenHeight();
    Position offset = (Position){editor->settings.command_padding.x, screen_height - editor->settings.command_padding.y - editor->settings.font_size};
    DrawTextEx(editor->settings.editor_font, ":", (Vector2){offset.x, offset.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color); 
    Vector2 offset_prefix = MeasureTextEx(editor->settings.editor_font, ":", editor->settings.font_size, 1); 
    if (editor->input_system.current_mode != MODE_COMMAND) {
        DrawTextEx(editor->settings.editor_font, editor->input_system.command_system.command_buffer, (Vector2){offset.x + offset_prefix.x, offset.y}, editor->settings.font_size, 1, editor->settings.scheme.command_color);    
    } else {
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
    EditorRenderCommand(editor);
    ModalSystemRender(editor);
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
        .command_color = WHITE,
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
    RegisterDefaultCommandBinding(&editor.input_system.command_system);
    
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