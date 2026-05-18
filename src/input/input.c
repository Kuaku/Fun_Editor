#include "input.h"
#include "../utils/utf8.h"

static KeyBinding default_normal_bindings[] = {
    { KEY_LEFT,  MODI_NONE,  ACTION_CURSOR_LEFT },
    { KEY_RIGHT, MODI_NONE,  ACTION_CURSOR_RIGHT },
    { KEY_UP,    MODI_NONE,  ACTION_CURSOR_UP },
    { KEY_DOWN,  MODI_NONE,  ACTION_CURSOR_DOWN },

    { KEY_LEFT,  MODI_CTRL,  ACTION_CURSOR_WORD_LEFT },
    { KEY_RIGHT, MODI_CTRL,  ACTION_CURSOR_WORD_RIGHT },

    { KEY_LEFT,  MODI_SHIFT, ACTION_SELECT_LEFT },
    { KEY_RIGHT, MODI_SHIFT, ACTION_SELECT_RIGHT },
    { KEY_UP,    MODI_SHIFT, ACTION_SELECT_UP },
    { KEY_DOWN,  MODI_SHIFT, ACTION_SELECT_DOWN },
    { KEY_LEFT,  MODI_CTRL | MODI_SHIFT, ACTION_SELECT_WORD_LEFT },
    { KEY_RIGHT, MODI_CTRL | MODI_SHIFT, ACTION_SELECT_WORD_RIGHT },
    { KEY_A,     MODI_CTRL,  ACTION_SELECT_ALL },

    { KEY_BACKSPACE, MODI_NONE, ACTION_DELETE_BACKWARD },
    { KEY_DELETE,    MODI_NONE, ACTION_DELETE_FORWARD },
    { KEY_ENTER,     MODI_NONE, ACTION_INSERT_NEWLINE },
    { KEY_TAB,       MODI_NONE, ACTION_INSERT_TAB },

    { KEY_C, MODI_CTRL, ACTION_COPY },
    { KEY_X, MODI_CTRL, ACTION_CUT },
    { KEY_V, MODI_CTRL, ACTION_PASTE },

    { KEY_Z, MODI_CTRL, ACTION_UNDO },
    { KEY_Y, MODI_CTRL, ACTION_REDO },
    { KEY_Z, MODI_CTRL | MODI_SHIFT, ACTION_REDO },

    { KEY_ESCAPE, MODI_NONE, ACTION_CANCEL },

    { KEY_P, MODI_CTRL, ACTION_OPEN_COMMAND_PALETTE},
    { KEY_G, MODI_CTRL, ACTION_GOTO},
    { KEY_F, MODI_CTRL, ACTION_SEARCH},
    { KEY_Q, MODI_CTRL, ACTION_QUIT },
    { KEY_B, MODI_CTRL, ACTION_OPEN_BUFFER_LIST },
    { KEY_O, MODI_CTRL, ACTION_OPEN_FILE },
    { KEY_E, MODI_CTRL, ACTION_OPEN_FILE_EXPLORER },
    { KEY_D, MODI_CTRL, ACTION_OPEN_STATISTICS },

    { KEY_S, MODI_CTRL, ACTION_SAVE},
};

static KeyBinding default_command_bindings[] = {
    { KEY_LEFT,  MODI_NONE,  ACTION_CURSOR_LEFT },
    { KEY_RIGHT, MODI_NONE,  ACTION_CURSOR_RIGHT },

    { KEY_BACKSPACE, MODI_NONE, ACTION_DELETE_BACKWARD },

    { KEY_P, MODI_CTRL, ACTION_OPEN_COMMAND_PALETTE},

    { KEY_ESCAPE, MODI_NONE, ACTION_CANCEL },

    { KEY_ENTER,     MODI_NONE, ACTION_EXECUTE_COMMAND },
};

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
        case ACTION_OPEN_FILE_EXPLORER: return "ACTION_OPEN_FILE_EXPLORER";
        case ACTION_OPEN_STATISTICS: return "ACTION_OPEN_STATISTICS";
        case ACTION_SAVE: return "ACTION_SAVE";
        default: return "UNKNOWN_ACTION";
    }
}

bool is_number(const char* str) {
    if (!str || *str == '\0') return false;

    const char* p = str;
    if (*p == '-') p++;

    if (*p == '\0') return false;

    while (*p) {
        if (!isdigit(*p)) return false;
        p++;
    }
    return true;
}

void ClearAction(Action* action) {
    if (action->text_buffer) {
        free(action->text_buffer);
    }
    action->length = 0;
}

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

void ClearInputSystem(InputSystem* system) {
    for (size_t i = 0; i < MODE_COUNT; i++) {
        if (system->bindings[i]) {
            free(system->bindings[i]);
        }
    }
    ClearCommandSystem(&system->command_system);
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

bool HasModifiers(ModifierFlags modifiers, ModifierFlags check) {
    return modifiers & check;
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

    int ch = GetCharPressed();
    if (ch != 0) {
        action.type = ACTION_INSERT_CHAR;
        action.text_buffer = malloc(5);
        size_t utf8_length = utf8_encode(ch, action.text_buffer);
        // TODO: Catch length = 0
        action.text_buffer[4] = '\0';
        action.length = utf8_length;
        return action;
    }

    int key = GetKeyPressed();
    if (key != 0) {
        ActionType found = LookupBinding(sys, key, mods);
        action.type = found;
        return action;
    }

    return action;
}

RawInput InputSystemPollRawInput() {
    RawInput input = {0};
    input.is_char = true;
    input.modifiers = GetCurrentModifiers();
    int ch = GetCharPressed();
    if (ch != 0) {
        input.key = ch;
        return input;
    }
    input.is_char = false;
    input.key = GetKeyPressed();
    return input;
}
