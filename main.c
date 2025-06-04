#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

typedef enum { ORIGINAL, ADD } BufferType;

typedef struct {
    BufferType source;
    size_t start;
    size_t length;
} Piece;

typedef struct {
    size_t x;
    size_t y;
} Position;

#define MAX_PIECES 1024
#define MAX_ADD_BUFFER 4096

char* org_buffer = "D";
char add_buffer[MAX_ADD_BUFFER];

size_t add_buffer_length = 0;
Piece pieces[MAX_PIECES];
size_t piece_count = 0;
bool dirtyPieces = false;

char* text_buffer = NULL;
size_t text_length = 0;
char** lines = NULL;
size_t line_count = 0;

size_t fontSize = 40;

size_t pointerPosition = 0;
size_t pointerPaddingX = 3, pointerPaddingY = 3, pointerWidth = 2;

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

void regenerate_text() {
    if (text_buffer != NULL) {
        free(text_buffer);
    }
    if (lines != NULL) {
        free(lines);
    }
    
    text_length = 0;
    line_count = 1;
    char* work_buffer;
    char* marker;
    for (size_t i = 0; i < piece_count; ++i) {
        text_length += pieces[i].length;
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        for (size_t j = 0; j < pieces[i].length; ++j) {
            if (work_buffer[pieces[i].start + j] == '\n') {
                line_count++;
            }
        }
    }
    text_length += 1;
    text_buffer = calloc(text_length + 1, sizeof(char));
    lines = calloc(line_count, sizeof(char*));
    lines[0] = text_buffer;

    marker = text_buffer;
    for (size_t i = 0; i < piece_count; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        work_buffer = work_buffer + pieces[i].start;
        memcpy(marker, work_buffer, pieces[i].length);
        marker = marker + pieces[i].length;
    }

    size_t line_index = 1;
    for (size_t i = 0; i < text_length - 1; ++i) {
        if (text_buffer[i] == '\n') {
            text_buffer[i] = 0;
            lines[line_index++] = text_buffer + i + 1;
        }
    }
    text_buffer[text_length] = '\0';

    dirtyPieces = false;
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

void RenderTextBuffer(size_t startX, size_t startY, size_t width, size_t height) {
    Position pointer = GetPointerPosition();
    BeginScissorMode(startX, startY, width, height);
    
    char* temp = NULL;
    size_t line_length;
    for (size_t i = 0; i < line_count; ++i) {
        if (pointer.y != i) {
            DrawText(lines[i], startX, startY + i * fontSize, fontSize, WHITE); 
        } else {       
            if (temp != NULL) {
                free(temp);
            } 
            line_length = strlen(lines[i]);
            temp = calloc(line_length + 1, sizeof(char));
            strncpy(temp, lines[i], pointer.x);
            int draw_length = MeasureText(temp, fontSize);
            DrawText(temp, startX, startY + i * fontSize, fontSize, WHITE);
            DrawText(lines[i] + pointer.x, startX + draw_length + pointerPaddingX * 2 + pointerWidth, startY + fontSize * i, fontSize, WHITE);   
            DrawRectangle(startX + draw_length + pointerPaddingX, startY + i * fontSize + pointerPaddingY, pointerWidth, fontSize - 2 * pointerPaddingY, WHITE);
        }
    }
    if (temp != NULL) {
        free(temp);
    }
    
    EndScissorMode();
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
    regenerate_text();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 700, "Fun Editor");
    SetTargetFPS(60); 

    while (!WindowShouldClose()) {
        size_t screenWidth = GetScreenWidth();
        size_t screenHeight = GetScreenHeight();

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

        if (dirtyPieces) {
            regenerate_text();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        RenderTextBuffer(40, 40, screenWidth - 40, screenHeight - 40);
        EndDrawing();
    }

    CloseWindow();
    free(org_buffer);
    return 0;
}
