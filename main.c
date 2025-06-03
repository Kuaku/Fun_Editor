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

size_t pointerX = 0;
size_t pointerY = 0;
size_t pointerPaddingX = 3, pointerPaddingY = 3, pointerWidth = 2;


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

int AppendAddBuffer(char value) {
    // TODO: Possible Buffer overflow!
    int result = add_buffer_length;
    add_buffer[add_buffer_length] = value;
    add_buffer_length++;
    return result;
}

size_t GetPointerPosition() {
    // TODO: Possible pointer to null (lines intial)
    size_t pointer_pos = 0;
    for (size_t i = 0; i < pointerY; ++i) {
        pointer_pos += strlen(lines[i]) + 1;
    }
    pointer_pos += pointerX;
    return pointer_pos;
}

void RemoveCharacter(size_t position) {
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
    }
}

void InsertCharacter(size_t position, char value) {
    size_t new_start = AppendAddBuffer(value);
    Piece new_piece = {ADD, new_start, 1};
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

void RenderTextBuffer(size_t startX, size_t startY, size_t width, size_t height) {
    BeginScissorMode(startX, startY, width, height);
    char* temp = NULL;
    size_t line_length;
    for (size_t i = 0; i < line_count; ++i) {
        if (pointerY != i) {
            DrawText(lines[i], startX, startY + i * fontSize, fontSize, WHITE); 
        } else {       
            if (temp != NULL) {
                free(temp);
            } 
            line_length = strlen(lines[i]);
            temp = calloc(line_length + 1, sizeof(char));
            strncpy(temp, lines[i], pointerX);
            int draw_length = MeasureText(temp, fontSize);
            DrawText(temp, startX, startY + i * fontSize, fontSize, WHITE);
            DrawText(lines[i] + pointerX, startX + draw_length + pointerPaddingX * 2 + pointerWidth, startY + fontSize * i, fontSize, WHITE);   
            DrawRectangle(startX + draw_length + pointerPaddingX, startY + i * fontSize + pointerPaddingY, pointerWidth, fontSize - 2 * pointerPaddingY, WHITE);
        }
    }
    EndScissorMode();
    if (temp != NULL) {
        free(temp);
    }
}

int main(void) {
    const int screenWidth = 1200;
    const int screenHeight = 700;

    pieces[0].source = ORIGINAL;
    pieces[0].start = 0;
    pieces[0].length = strlen(org_buffer);
    piece_count = 1;
    regenerate_text();

    InitWindow(screenWidth, screenHeight, "Fun Editor");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_LEFT) && pointerX > 0) {
            pointerX--;
        }
        if (IsKeyPressed(KEY_RIGHT) && pointerX < strlen(lines[pointerY])) {
            pointerX++;
        }
        if (IsKeyPressed(KEY_UP) && pointerY > 0) {
            pointerY--;
            pointerX = min(pointerX, strlen(lines[pointerY]));
        } 
        if (IsKeyPressed(KEY_DOWN) && pointerY < line_count - 1) {
            pointerY++;
            pointerX = min(pointerX, strlen(lines[pointerY]));
        } 


        if (IsKeyPressed(KEY_BACKSPACE)) {
            RemoveCharacter(GetPointerPosition());
            if (pointerX == 0) {
                if (pointerY > 0) {
                    pointerY--;
                    pointerX = strlen(lines[pointerY]);
                }
            } else {
                pointerX--;
            }
        }

        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126) {
                InsertCharacter(GetPointerPosition(), key);
                pointerX++;
            }
            key = GetCharPressed();
        }       
        if (IsKeyPressed(KEY_ENTER)) {    
            InsertCharacter(GetPointerPosition(), '\n');
            pointerX=0;
            pointerY++;
        }
        if (dirtyPieces) {
            regenerate_text();
        }

        BeginDrawing();
        {
            ClearBackground(BLACK);
            RenderTextBuffer(screenWidth/4.0, screenHeight/4.0, screenWidth/2.0, screenHeight/2.0); 
        }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
