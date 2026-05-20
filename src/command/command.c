#include "command.h"
#include "../editor/editor.h"
#include "../input/input.h"
#include "../utils/utf8.h"

void ClearCommandToken(CommandToken* token) {
    if (token->char_value) {
        free(token->char_value);
    }
    token->numb_value = 0;
}

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
    uint32_t codepoint;
    size_t length = utf8_decode(system->command_buffer + system->pointer_position, &codepoint);

    memmove(system->command_buffer + system->pointer_position,
            system->command_buffer + system->pointer_position + length,
            command_buffer_length - system->pointer_position - length + 1);
}

void CommandSystemBackspace(CommandSystem* system) {
    if (system->pointer_position == 0) {
        return;
    }
    int start_position = system->pointer_position;
    do {
        system->pointer_position--;
    } while (system->pointer_position > 0 && utf8_is_continuation(system->command_buffer[system->pointer_position]));

    CommandSystemRemoveChar(system);
}

void MoveCommandPointerLeft(CommandSystem* system) {
    if (system->pointer_position <= 0) return;
    do {
        system->pointer_position--;
    } while (system->pointer_position > 0 && utf8_is_continuation(system->command_buffer[system->pointer_position]));
}

void MoveCommandPointerRight(CommandSystem* system) {
    size_t command_buffer_length = strlen(system->command_buffer);
    if (system->pointer_position >= command_buffer_length) return;
    do {
        system->pointer_position++;
    } while(system->pointer_position <= command_buffer_length - 1 && utf8_is_continuation(system->command_buffer[system->pointer_position]));
}

void AddCommandBinding(CommandSystem* system, CommandBinding binding) {
    while (system->command_bindings_count >= system->command_bindings_capacity) {
        system->bindings = realloc(system->bindings, system->command_bindings_capacity * 2 * sizeof(CommandBinding));
        system->command_bindings_capacity *= 2;
    }
    system->bindings[system->command_bindings_count++] = binding;
}

void ResetCommandBuffer(CommandSystem* system) {
    memset(system->command_buffer, 0, system->command_buffer_capacity);
    system->pointer_position = 0;
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

void GotoCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    if (tokens[0].numb_value == 0) {
        return;
    }
    size_t line = tokens[0].numb_value - 1;
    if (line >= 0 && line < GetLineCount(buffer)) {
        buffer->pointer_position = GetLineByIndex(buffer, line).x;
        ResetCommandBuffer(&editor->input_system.command_system);
        editor->input_system.current_mode = MODE_TEXT;
    }
}

void FindCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    TextBuffer* buffer = GetActiveBuffer(editor);
    size_t line_counter = GetPointerPosition(buffer).y + 1;
    size_t line_count = GetLineCount(buffer);
    size_t search_length = strlen(tokens[0].char_value);

    if (line_count == 0) return;
    if (search_length == 0) return;

    Position found = {-1, -1};
    for (size_t i = 0; i < line_count; ++i) {
        size_t working_line = (line_counter + i) % line_count;
        char* line = GenerateLine(buffer, working_line);
        size_t line_length = strlen(line);

        if (line_length < search_length) {
            free(line);
            continue;
        }

        for (size_t j = 0; j <= line_length - search_length; ++j) {
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

void OpenCommand(Editor* editor, CommandToken* tokens, size_t token_count) {
    struct stat st;
    if (stat(tokens[0].char_value, &st) != 0 || !S_ISREG(st.st_mode)) {
        return;
    }
    OpenFileFromPath(editor, tokens[0].char_value);
    ResetCommandBuffer(&editor->input_system.command_system);
    editor->input_system.current_mode = MODE_TEXT;
}

void QuitCommando(Editor* editor, CommandToken* tokens, size_t token_count) {
    editor->state.exit_requested = true;
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
