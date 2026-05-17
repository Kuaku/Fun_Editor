#ifndef INPUT_H
#define INPUT_H

#include "../common.h"
#include "../command/command.h"

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

    ACTION_OPEN_FILE_EXPLORER,
    ACTION_OPEN_STATISTICS,

    ACTION_EXECUTE_COMMAND,

    ACTION_SAVE,
} ActionType;

typedef enum {
    MODI_NONE  = 0,
    MODI_CTRL  = 1 << 0,
    MODI_SHIFT = 1 << 1,
    MODI_ALT   = 1 << 2,
    MODI_SUPER = 1 << 3,
} ModifierFlags;

typedef struct {
    ActionType type;
    char* text_buffer;
    size_t length;
} Action;

typedef struct {
    int key;
    ModifierFlags mods;
    ActionType action;
} KeyBinding;

typedef struct {
    ModifierFlags modifiers;
    int key;
    bool is_char;
} RawInput;

typedef struct {
    EditorMode current_mode;

    KeyBinding* bindings[MODE_COUNT];
    size_t binding_counts[MODE_COUNT];

    CommandSystem command_system;
} InputSystem;

const char* ActionTypeToString(ActionType type);
bool is_number(const char* str);
void ClearAction(Action* action);

InputSystem InitInputSystem(void);
void ClearInputSystem(InputSystem* system);

ModifierFlags GetCurrentModifiers(void);
bool HasModifiers(ModifierFlags modifiers, ModifierFlags check);
ActionType LookupBinding(InputSystem* sys, int key, ModifierFlags mods);

Action InputSystemPoll(InputSystem* sys);
RawInput InputSystemPollRawInput(void);

#endif
