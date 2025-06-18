#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include <time.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

typedef enum { ORIGINAL, ADD } BufferType;
typedef enum { TOP, BOTTOM } AnchorType;

typedef struct {
    BufferType source;
    size_t start;
    size_t length;
} Piece;

typedef struct {
    size_t x;
    size_t y;
} Position;

typedef struct {
    char** args;
    size_t count;
} CommandArgs;

#define MAX_PIECES 1024
#define MAX_ADD_BUFFER 4096
#define MAX_COMMAND_BUFFER 4096

#define BackgroundColor BLACK
#define TextColor       WHITE
#define ModeColor       WHITE
#define CommandColor    WHITE
#define LineNumberColor YELLOW

char* org_buffer = "";
char add_buffer[MAX_ADD_BUFFER];

size_t add_buffer_length = 0;
Piece pieces[MAX_PIECES];
size_t piece_count = 0;
bool dirtyPieces = false;

size_t line_anchor = 0;
size_t offsetX = 0;

size_t fontSize = 30;

size_t pointerPosition = 0;
size_t pointerPaddingX = 3, pointerPaddingY = 3, pointerWidth = 2;

size_t numberPadding = 10;

Font editor_font;


size_t commando_pointer_position = 0;
char commando_content[MAX_COMMAND_BUFFER] = "";
size_t command_length = 0;
bool is_command_mode = false;

size_t mode_padding = 10;
size_t command_padding = 10;
CommandArgs command_args;

void LogAddBuffer() {
    printf("AddBuffer: ");
    for (size_t i = 0; i < add_buffer_length; ++i) {
        if (add_buffer[i] == ' ') {
            printf("Space ");
        } else if (add_buffer[i] == '\n') {
            printf("Enter ");
        } else {
            printf("%c ", add_buffer[i]);
        }
    }
    printf("\n");
}

void LogPieces() {
    TraceLog(LOG_INFO, "===== PIECES =====");
    for (size_t i = 0; i < piece_count; ++i) {
        Piece p = pieces[i];
        const char* sourceName = (p.source == ORIGINAL) ? "ORIGINAL" : "ADD";
        const char* buffer = (p.source == ORIGINAL) ? org_buffer : add_buffer;

        // Sichere Kopie für null-terminierten Text
        char text[1025] = {0};
        size_t copyLen = (p.length < 1024) ? p.length : 1024;
        strncpy(text, buffer + p.start, copyLen);
        text[copyLen] = '\0'; // sicherstellen, dass nullterminiert

        TraceLog(LOG_INFO, "[%zu] Source: %s, Start: %zu, Length: %zu, Text: \"%s\"",
                 i, sourceName, p.start, p.length, text);
    }
    TraceLog(LOG_INFO, "==================");
}

char* GenerateText(size_t* out_length) {
    size_t buffer_length = 0;
    for (size_t i = 0; i < piece_count; ++i) {
        buffer_length += pieces[i].length;
    }

    char* out = calloc(buffer_length + 1, sizeof(char));
    char* work_buffer;
    char* marker = out;
    for (size_t i = 0; i < piece_count; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        work_buffer = work_buffer + pieces[i].start;
        memcpy(marker, work_buffer, pieces[i].length);
        marker = marker + pieces[i].length;
    }

    *out_length = buffer_length;
    return out;
}

Position GetLineByIndex(size_t index) {
    Position out = {0, -1};
    char* work_buffer;
    size_t current_line = 0;
    size_t traversed = 0;
    for (size_t i = 0; i < piece_count && out.y == -1; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        for (size_t j = 0; j < pieces[i].length; ++j) {
            if (work_buffer[pieces[i].start + j] == '\n') {
                current_line++;
                if (current_line == index) {
                    out.x = traversed + j + 1;
                } else if (current_line == index + 1) {
                    out.y = traversed + j - out.x;
                    break;
                }
            }
        }
        traversed += pieces[i].length;
        if (out.y != -1) {
            break;
        }
    }
    if (out.y == -1) {
        out.y = traversed - out.x;
    }
    return out;
}

char* GenerateLine(size_t index) {
    char* line;
    Position line_position = GetLineByIndex(index);
    line = calloc(line_position.y + 1, sizeof(char));

    size_t start_pos = line_position.x;
    size_t end_pos = start_pos + line_position.y;

    size_t traversed = 0;
    char* temp = line;
    Piece piece;
    char* work_buffer;
    size_t copied = 0;
    for (size_t i = 0; i < piece_count && copied < line_position.y; ++i) {
        piece = pieces[i];
        work_buffer = piece.source == ORIGINAL ? org_buffer : add_buffer;
        
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

int AppendAddBuffer(char* value, size_t len) {
    // TODO: Possible Buffer overflow!
    int result = add_buffer_length;
    memcpy(&add_buffer[add_buffer_length], value, len);
    add_buffer_length += len;
    return result;
}

size_t GetTextSize() {
    size_t out = 0;
    for (size_t i = 0; i < piece_count; ++i) {
        out += pieces[i].length;
    }
    return out;
}

Position GetPointerPosition() {
    Position out = {0, 0};
    size_t traversed = 0;
    char* work_buffer;
    for (size_t i = 0; i < piece_count && traversed < pointerPosition; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        
        size_t to_read = pointerPosition - traversed;
        if (to_read > pieces[i].length) to_read = pieces[i].length;
        for (size_t j = 0; j < to_read; ++j) {
            if (work_buffer[pieces[i].start + j] == '\n') {
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

bool RemoveCharacter(size_t position) {
    if (position > 0) {
        Piece new_pieces[MAX_PIECES];
        int new_count = 0;
        size_t current_pos = 0;

        position--;

        for (size_t i = 0; i < piece_count; ++i) {
            Piece p = pieces[i];

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
        memcpy(pieces, new_pieces, sizeof(Piece) * new_count);
        piece_count = new_count;
        dirtyPieces = true; 
        return true;
    }
    return false;
}

void RemoveCharacterAtPointer() {
    if (RemoveCharacter(pointerPosition)) {
        pointerPosition--;
    }
}

void InsertString(size_t position, char* value, size_t len) {
    size_t new_start = AppendAddBuffer(value, len);
    Piece new_piece = {ADD, new_start, len};
    Piece new_pieces[MAX_PIECES];
    size_t new_count = 0;
    size_t current_pos = 0;
    if (piece_count == 0) {
        new_pieces[new_count++] = new_piece;
    }
    for (size_t i = 0; i < piece_count; ++i) {
        Piece p = pieces[i];
        if (current_pos + p.length < position) {
            new_pieces[new_count++] = p;
            current_pos += p.length;
        } else {
            size_t offset = position - current_pos;
            if (offset > 0) {
                Piece left = p;
                left.source = p.source;
                left.length = offset;
                new_pieces[new_count++] = left;
            }

            new_pieces[new_count++] = new_piece;

            if (offset < p.length) {
                Piece right = p;
                right.source = p.source;
                right.start += offset;
                right.length -= offset;
                new_pieces[new_count++] = right;
            }

            for (size_t j = i + 1; j < piece_count; ++j) {
                new_pieces[new_count++] = pieces[j];
            }
            break;
        }
    }
    memcpy(pieces, new_pieces, sizeof(Piece) * new_count);
    piece_count = new_count;
    dirtyPieces = true;
}

void InsertStringAtPointer(char* value, size_t len) {
    InsertString(pointerPosition, value, len);
    pointerPosition += len;
}

void InsertCharacter(size_t position, char value) {
    InsertString(position, &value, 1);
}

void InsertCharacterAtPointer(char value) {
    InsertCharacter(pointerPosition, value);
    pointerPosition += 1;
}

size_t GetPointerOffsetFromLeft(Position pointer) {
    char* temp = NULL;
    char* line = GenerateLine(pointer.y);
    size_t line_length = strlen(line);
    temp = calloc(line_length+1, sizeof(char));

    strncpy(temp, line, pointer.x);
    Vector2 draw_length = MeasureTextEx(editor_font, temp, fontSize, 1);
    
    free(temp);
    free(line);
    return draw_length.x;
}

void RenderLine(int xX, int yY, size_t index, Position pointer) {
    char* temp = NULL;
    size_t line_buffer_length;
    char* line = GenerateLine(index);
    size_t line_length;
    if (pointer.y != index) {
            DrawTextEx(editor_font, line, (Vector2){xX, yY}, fontSize, 1, TextColor); 
    } else {       
        if (temp != NULL) {
            free(temp);
        } 
        line_length = strlen(line);
        temp = calloc(line_length + 1, sizeof(char));
        strncpy(temp, line, pointer.x);
        Vector2 draw_length = MeasureTextEx(editor_font, temp, fontSize, 1);
        DrawTextEx(editor_font, temp, (Vector2){xX, yY}, fontSize, 1, TextColor);
        DrawTextEx(editor_font, line + pointer.x, (Vector2){xX + draw_length.x + pointerPaddingX * 2 + pointerWidth, yY}, fontSize, 1, TextColor);   
        DrawRectangle(xX + draw_length.x + pointerPaddingX, yY, pointerWidth, fontSize - 2 * pointerPaddingY, TextColor);
    }
    free(line);
    if (temp != NULL) {
        free(temp);
    }
}

size_t GetLineCount()  {
    size_t out = 1;
    char* work_buffer;
    for (size_t i = 0; i < piece_count; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        for (size_t j = 0; j < pieces[i].length; ++j) {
            if (work_buffer[pieces[i].start + j] == '\n') {
                out++;
            }
        }
    }
    return out;
}

void RenderTextBuffer(size_t startX, size_t startY, size_t width, size_t height) {
    Position pointer = GetPointerPosition();
    size_t pointer_offset = GetPointerOffsetFromLeft(pointer);
    size_t lines_completly_rendered = height / fontSize;size_t line_number = line_anchor;
    size_t line_count = GetLineCount();

    BeginScissorMode(startX, startY, width, height);
    if (pointer.y >= line_anchor + lines_completly_rendered) {
        line_anchor = pointer.y - lines_completly_rendered + 1;
    }
    if (pointer.y <= line_anchor) {
        line_anchor = pointer.y;
    }

    if (offsetX + width <= pointer_offset) {
        offsetX = pointer_offset - width + pointerPaddingX * 2 + pointerWidth;
    }
    if (offsetX > pointer_offset) {
        offsetX = pointer_offset;
    }

    size_t line_y = 0;
    for (size_t i = line_anchor; i < min(line_anchor + lines_completly_rendered + 1, line_count); ++i) {    
        RenderLine(startX-offsetX, startY + line_y * fontSize, i, pointer);
        line_y++;
    }
    EndScissorMode();
}

void RenderTextField(size_t startX, size_t startY, size_t width, size_t height) {
    Position pointer = GetPointerPosition();
    size_t pointer_offset = GetPointerOffsetFromLeft(pointer);
    size_t lines_completly_rendered = height / fontSize;size_t line_number = line_anchor;
    size_t line_count = GetLineCount();

    size_t max_offset = 0;
    size_t digits = snprintf(NULL, 0, "%zu", min(line_anchor + lines_completly_rendered + 1, line_count) + 1);
    size_t local_offset = 0;
    Vector2 measured_text;
    char* number_str  = malloc(digits + 1);
    for (size_t i = line_anchor; i < min(line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = MeasureTextEx(editor_font, number_str, fontSize, 1);
        local_offset = measured_text.x;
        if (local_offset > max_offset) {
            max_offset = local_offset;
        }
    }
    max_offset += numberPadding * 2;

    RenderTextBuffer(startX + max_offset, startY, width - max_offset, height);

    BeginScissorMode(startX, startY, width, height);
    size_t line_y = 0;
    for (size_t i = line_anchor; i < min(line_anchor + lines_completly_rendered + 1, line_count); ++i) {
        snprintf(number_str, digits + 1, "%zu", i + 1);
        measured_text = MeasureTextEx(editor_font, number_str, fontSize, 1);
        local_offset = measured_text.x;
        DrawTextEx(editor_font, number_str, (Vector2){startX + max_offset - numberPadding - local_offset, startY + line_y * fontSize}, fontSize, 1, LineNumberColor);
        line_y++;
    }
    EndScissorMode();
    free(number_str);
}

void RenderMode() {
    char* mode;
    if (is_command_mode) {
        mode = "Command Mode";
    } else {
        mode = "Text Mode";
    }
    DrawTextEx(editor_font, mode, (Vector2){mode_padding, mode_padding}, fontSize, 1, ModeColor);
}

void RenderCommand(size_t offsetX, size_t offsetY) {
    DrawTextEx(editor_font, ":", (Vector2){offsetX, offsetY}, fontSize, 1, CommandColor); 
    Vector2 offset_prefix = MeasureTextEx(editor_font, ":", fontSize, 1); 
    if (!is_command_mode) {
        DrawTextEx(editor_font, commando_content, (Vector2){offsetX + offset_prefix.x, offsetY}, fontSize, 1, CommandColor);    
    } else {
        char* temp;
        temp = calloc(commando_pointer_position + 1, sizeof(char));
        strncpy(temp, commando_content, commando_pointer_position);
        DrawTextEx(editor_font, temp, (Vector2){offsetX + offset_prefix.x, offsetY}, fontSize, 1, CommandColor);
        Vector2 offset_first_part = MeasureTextEx(editor_font, temp, fontSize, 1);
        DrawRectangle(offsetX + offset_prefix.x + offset_first_part.x + pointerPaddingX, offsetY + pointerPaddingY, pointerWidth, fontSize - pointerPaddingY * 2, WHITE);
        char* last_part = commando_content + commando_pointer_position;
        DrawTextEx(editor_font, last_part, (Vector2){offsetX + offset_prefix.x + offset_first_part.x + pointerPaddingX * 2 + pointerWidth, offsetY}, fontSize, 1, CommandColor);
        free(temp);
    }
}

char* load_file(const char* filename, size_t* out_len) {
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
    buf[len] = '\0'; // Null-terminate for safety
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

void save_file(const char* filename, char* content, size_t length) {
    FILE* file = fopen(filename, "wb");
   if (file == NULL) {
        fprintf(stderr, "Could not open file: %s\n", filename); 
        return;
   }

   size_t bytes_written = fwrite(content, 1, length, file);
   if (bytes_written != length) {
       fprintf(stderr, "Error: Could not write all data to file '%s'\n", filename);
   }
   fclose(file);
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

void jumpLineUp() {
    Position pointer = GetPointerPosition();
    if (pointer.y == 0) {
        return;
    }
    Position nextLine = GetLineByIndex(pointer.y - 1);
    pointerPosition = nextLine.x + min(nextLine.y, pointer.x);
}

void jumpLineDown() {
    Position pointer = GetPointerPosition();
    size_t max_lines = GetLineCount();
    if (pointer.y >= max_lines - 1) {
        return;
    }
    Position nextLine = GetLineByIndex(pointer.y + 1);
    pointerPosition = nextLine.x + min(nextLine.y, pointer.x);
}

void ResetCommandArgs(CommandArgs* args) {
    if ((*args).args) {
        for (size_t i = 0; i < (*args).count; ++i) {
            free((*args).args[i]);
        }
        free((*args).args);
    }
    (*args).args = NULL;
    (*args).count = 0;
}

char* SkipWhiteSpace(char* input) {
    char* out = input;
    while (*out == ' ' || *out == '\t') out++;
    return out;
}

size_t CountArgs(char* command, size_t lenght) {
    char* input = SkipWhiteSpace(command);
    if (*input == '\0') return 0;
    
    char* temp = input;
    size_t count = 0;
    bool in_quotes = false;
    bool in_word = false;

    while (*temp) {
        if (*temp == '"') {
            in_quotes = !in_quotes;
            if (!in_word) {
                count++;
                in_word = true;
            }
        } else if ((*temp == ' ' || *temp == '\t') && !in_quotes) {
            in_word = false;
        } else if (!in_word) {
            count++;
            in_word = true;
        }
        temp++;
    }

    return count;
}

void ParseCommandArgs() {
    ResetCommandArgs(&command_args);
    size_t count = CountArgs(commando_content, command_length);
    char* input = SkipWhiteSpace(commando_content);
    char* start = input;
    command_args.args = malloc(count * sizeof(char*));
    bool in_quotes = false;
    bool in_word = false;
    while (*input && command_args.count < count) {
        if (*input == '"') {
            if (!in_word) {
                start = input + 1;
                in_word = true;
            }
            in_quotes = !in_quotes;
            if (!in_quotes) {
                size_t len = input - start;
                command_args.args[command_args.count] = malloc(len + 1);
                strncpy(command_args.args[command_args.count], start, len);
                command_args.args[command_args.count][len] = '\0';
                command_args.count++;
                in_word = false;
            }
        } else if ((*input == ' ' || *input == '\t') && !in_quotes) {
            if (in_word && !in_quotes) {
                size_t len = input - start;
                command_args.args[command_args.count] = malloc(len + 1);
                strncpy(command_args.args[command_args.count], start, len);
                command_args.args[command_args.count][len] = '\0';
                command_args.count++;
                in_word = false;
            }
        } else if (!in_word) {
            start = input;
            in_word = true;
        }
        input++;
    }

    if (in_word && !in_quotes) {
        size_t len = input - start;
        command_args.args[command_args.count] = malloc(len + 1);
        strncpy(command_args.args[command_args.count], start, len);
        command_args.args[command_args.count][len] = '\0';
        command_args.count++;
    }
}

void ExecuteCommand() {
    if (command_args.count < 1) return;
    if (strcmp(command_args.args[0], "find") == 0) {
        if (command_args.count != 2) return;

    } else if (strcmp(command_args.args[0], "goto") == 0) {
        if (command_args.count != 2) return;

        size_t line = atoi(command_args.args[1]) - 1;
        if (line >= 0 && line < GetLineCount()) {
            pointerPosition = GetLineByIndex(line).x;
            is_command_mode = false;
            command_length = 0;
            memset(commando_content, 0, MAX_COMMAND_BUFFER);
            commando_pointer_position = 0;
        }
    }
}

int main(int argc, char** argv) {

    size_t org_buffer_length = 0;

    if (argc >= 2) {
        org_buffer = load_file(argv[1], &org_buffer_length);
        if (!org_buffer) {
            fprintf(stderr, "Failed to load file, using default text.\n");
            org_buffer = strdup("");
        }
        org_buffer_length = strlen(org_buffer);
        normalize_line_endings(org_buffer);
    } else {
        org_buffer = strdup("");
        org_buffer_length = strlen(org_buffer);
    }

    pieces[0].source = ORIGINAL;
    pieces[0].start = 0;
    pieces[0].length = strlen(org_buffer);
    piece_count = 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 700, "Fun Editor");
    SetTargetFPS(60); 

    editor_font = LoadFontEx("Input.ttf", fontSize, 0, 250);

    while (!WindowShouldClose()) {
        size_t screenWidth = GetScreenWidth();
        size_t screenHeight = GetScreenHeight();

        if (!is_command_mode) {
            if (IsKeyPressed(KEY_LEFT) && pointerPosition > 0) {
                pointerPosition--;
            }
            if (IsKeyPressed(KEY_RIGHT) && pointerPosition <= GetTextSize()) {
                pointerPosition++;
            }
            if (IsKeyPressed(KEY_UP)) {
                jumpLineUp();
            } 
            if (IsKeyPressed(KEY_DOWN)) {
                jumpLineDown();
            } 


            if (IsKeyPressed(KEY_BACKSPACE)) {
                RemoveCharacterAtPointer();
            }

            if (IsKeyDown(KEY_LEFT_CONTROL)) {
                if (IsKeyPressed(KEY_S)) {
                    size_t length;
                    char* text = GenerateText(&length);
                    if (argc >= 2) {
                        save_file(argv[1], text, length);
                    }
                    // TODO: Clean up org and add puffer and compress piece table
                }

                if (IsKeyPressed(KEY_P)) {
                    is_command_mode = true;
                }
                
                if (IsKeyPressed(KEY_F)) {
                    is_command_mode = true;
                    memset(commando_content, 0, MAX_COMMAND_BUFFER);
                    strcpy(commando_content, "find \"\"");
                    command_length = strlen("find \"\"");
                    commando_pointer_position = command_length - 1;
                }
                
                if (IsKeyPressed(KEY_G)) {
                    is_command_mode = true;
                    memset(commando_content, 0, MAX_COMMAND_BUFFER);
                    strcpy(commando_content, "goto ");
                    command_length = strlen("goto ");
                    commando_pointer_position = command_length;
                }
            } else {
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 126) {
                        InsertCharacterAtPointer(key);
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_TAB)) {
                    InsertStringAtPointer("  ", 2);
                }       
                if (IsKeyPressed(KEY_ENTER)) {    
                    InsertCharacterAtPointer('\n');
                }
            }
        } else {
            if (IsKeyDown(KEY_LEFT_CONTROL)) {
                if (IsKeyPressed(KEY_P)) {
                    is_command_mode = false;
                }
            } else {
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 126) {
                        memmove(commando_content + commando_pointer_position + 1, commando_content + commando_pointer_position, MAX_COMMAND_BUFFER - commando_pointer_position - 1);
                        commando_content[commando_pointer_position] = key;
                        command_length++;
                        commando_pointer_position++;
                    }
                    key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && commando_pointer_position > 0) {
                    memmove(commando_content + commando_pointer_position - 1, 
                        commando_content + commando_pointer_position, 
                        MAX_COMMAND_BUFFER - commando_pointer_position);
                    command_length--;
                    commando_pointer_position--;
                }

                if (IsKeyPressed(KEY_LEFT) && commando_pointer_position > 0) {
                    commando_pointer_position--;
                }
                if (IsKeyPressed(KEY_RIGHT) && commando_pointer_position < command_length) {
                    commando_pointer_position++;
                }

                if (IsKeyPressed(KEY_ENTER)) {
                    ParseCommandArgs();
                    ExecuteCommand();
                }


            }
        }

        BeginDrawing(); 
        ClearBackground(BackgroundColor);
        RenderMode();
        clock_t start = clock();
        RenderTextField(0, mode_padding * 2 + fontSize, screenWidth - 40, screenHeight - (mode_padding * 2 + fontSize * 2 + command_padding * 2));
        clock_t end = clock();
        double time = ((double)(end-start)) / CLOCKS_PER_SEC * 1000.0;
        printf("[TIMING] RenderTextField: %.3fms\n", time); 
        RenderCommand(command_padding, screenHeight - command_padding - fontSize);
        EndDrawing();
    }

    CloseWindow();
    free(org_buffer);
    return 0;
}
