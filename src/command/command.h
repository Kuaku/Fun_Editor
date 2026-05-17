#ifndef COMMAND_H
#define COMMAND_H

#include "../common.h"

typedef enum {
    TOKENTYPE_STRING,
    TOKENTYPE_NUMBER
} CommandTokenType;

typedef struct {
    CommandTokenType type;
    char* char_value;
    int numb_value;
} CommandToken;

typedef struct {
    CommandToken* tokens;
    size_t capacity;
    size_t count;
} TokenList;

typedef void (*CommandExecuteFunc)(Editor* editor, CommandToken* tokens, size_t token_count);

typedef struct {
    char* command;
    CommandTokenType* needed_types;
    size_t needed_types_count;
    CommandExecuteFunc execute;
} CommandBinding;

typedef struct {
    char* command_buffer;
    size_t command_buffer_capacity;

    CommandBinding* bindings;
    size_t command_bindings_capacity;
    size_t command_bindings_count;

    size_t pointer_position;
} CommandSystem;

void ClearCommandToken(CommandToken* token);
TokenList Tokenize(const char* command_buffer);
void ClearTokenList(TokenList* list);

CommandBinding CreateCommandBinding(const char* command, CommandTokenType* needed_types, size_t needed_types_count, CommandExecuteFunc execute);
void ClearCommandBinding(CommandBinding* binding);

CommandSystem InitCommandSystem(void);
void ClearCommandSystem(CommandSystem* system);

void CommandSystemInsertString(CommandSystem* system, char* value, size_t len);
void CommandSystemRemoveChar(CommandSystem* system);
void CommandSystemBackspace(CommandSystem* system);
void MoveCommandPointerLeft(CommandSystem* system);
void MoveCommandPointerRight(CommandSystem* system);

void AddCommandBinding(CommandSystem* system, CommandBinding binding);
void ResetCommandBuffer(CommandSystem* system);

void TryExecuteCommandSystem(Editor* editor);
void EnterCommandModeWithCommand(Editor* editor, const char* command, size_t pointer_position);

void GotoCommand(Editor* editor, CommandToken* tokens, size_t token_count);
void FindCommand(Editor* editor, CommandToken* tokens, size_t token_count);
void OpenCommand(Editor* editor, CommandToken* tokens, size_t token_count);
void QuitCommando(Editor* editor, CommandToken* tokens, size_t token_count);

void RegisterDefaultCommandBinding(CommandSystem* system);

#endif
