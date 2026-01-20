#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include <time.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

typedef struct {
    Position* line_positions;
    size_t line_count;
    size_t capacity;
    bool is_valid;
} LineCache;

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

typedef struct {
    EditEntry* entries;
    size_t count;
    size_t capacity;
    size_t current;
} UndoStack;

#define MAX_PIECES 1024
#define MAX_ADD_BUFFER 4096
#define MAX_COMMAND_BUFFER 4096

#define BackgroundColor (Color){32, 35, 41, 255}
#define TextColor       WHITE
#define ModeColor       WHITE
#define CommandColor    WHITE
#define LineNumberColor YELLOW

char* org_buffer = "";
char add_buffer[MAX_ADD_BUFFER];

size_t add_buffer_length = 0;
Piece pieces[MAX_PIECES];
size_t piece_count = 0;
LineCache line_cache = {0};

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
bool exit_requested = false;

size_t mode_padding = 10;
size_t command_padding = 10;
CommandArgs command_args;


UndoStack undoStack = {0};
bool can_merge_with_previous = false;
double time_since_last_edit = 0;

size_t selection_start = 0;
size_t selection_end = 0;
bool has_selection = false;

void InitUndoStack()  {
    undoStack.capacity = 10000;
    undoStack.entries = calloc(undoStack.capacity, sizeof(EditEntry));
    undoStack.count = 0;
    undoStack.current = 0;
}

void FreeEditEntry(EditEntry* entry) {
    if (entry->text) {
        free(entry->text);
        entry->text = NULL;
    }
}

void PushCommand(EditType type, size_t position, const char* text, size_t length) {
    for (size_t i = undoStack.current; i < undoStack.count; i++) {
        FreeEditEntry(&undoStack.entries[i]);      
    }
    undoStack.count = undoStack.current;

    if (undoStack.count >= undoStack.capacity) {
        FreeEditEntry(&undoStack.entries[0]);
        memmove(undoStack.entries, undoStack.entries + 1, (undoStack.capacity - 1) * sizeof(EditEntry));
        undoStack.count--;
    }
    
    EditEntry* entry = &undoStack.entries[undoStack.count];
    entry->type = type;
    entry->position = position;
    entry->length = length;
    entry->cursor_before = pointerPosition;
    switch (entry->type)
    {
        case EDIT_INSERT:
            entry->cursor_after = pointerPosition + entry->length;
            break;
        case EDIT_DELETE:
            entry->cursor_after = pointerPosition - entry->length;
            break;
    }

    if (text && length > 0) {
        entry->text = malloc(length + 1);
        memcpy(entry->text, text, length);
        entry->text[length] = '\0';
    } else {
        entry->text = NULL;
    }

    undoStack.count++;
    undoStack.current = undoStack.count;
}

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

void InitLineCache() {
    line_cache.capacity = 1024;
    line_cache.line_positions = calloc(line_cache.capacity, sizeof(Position));
    line_cache.line_count = 0;
    line_cache.is_valid = false;
}

void FreeLineCache() {
    if (line_cache.line_positions) {
        free(line_cache.line_positions);
        line_cache.line_positions = NULL;
    }

    line_cache.line_count = 0;
    line_cache.capacity = 0;
    line_cache.is_valid = false;
}

void InvalidateLineCache() {
    line_cache.is_valid = false;
}

void RebuildLineCache() {
    line_cache.line_count = 0;
    line_cache.line_positions[0].x = 0;
    line_cache.line_positions[0].y = 0;
    size_t current_pos = 0;
    char* work_buffer;

    for (size_t i = 0; i < piece_count; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;

        for (size_t j = 0; j < pieces[i].length; ++j) {
            if (work_buffer[pieces[i].start + j] == '\n') {
                
                if (line_cache.line_count + 1 >= line_cache.capacity) {
                    line_cache.capacity *= 2;
                    line_cache.line_positions = realloc(line_cache.line_positions, line_cache.capacity * sizeof(Position));
                }

                line_cache.line_positions[line_cache.line_count].y = current_pos - line_cache.line_positions[line_cache.line_count].x;
                line_cache.line_count++;
                line_cache.line_positions[line_cache.line_count].x = current_pos + 1;
                line_cache.line_positions[line_cache.line_count].y = 0;
            }
            current_pos++;
        }
    }
    line_cache.line_positions[line_cache.line_count].y = current_pos - line_cache.line_positions[line_cache.line_count].x;
    line_cache.line_count++;
    line_cache.is_valid = true;
}

Position GetLinePosition(size_t index) {
    if (!line_cache.is_valid) {
        RebuildLineCache();
    }

    return line_cache.line_positions[index];
}


Position GetLineByIndex(size_t index) {
    return GetLinePosition(index);
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
        InvalidateLineCache();
        return true;
    }
    return false;
}

char GetCharAt(size_t position) {
    size_t traversed = 0;
    char* work_buffer;
    
    for (size_t i = 0; i < piece_count; i++) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        
        if (traversed + pieces[i].length > position) {
            size_t offset = position - traversed;
            return work_buffer[pieces[i].start + offset];
        }
        
        traversed += pieces[i].length;
    }
    
    return '\0'; 
}

bool TryToMergeCharacterRemove(float current_time) {
    if (undoStack.current > 0 && current_time - time_since_last_edit < 1.0) {
        EditEntry* prev = &undoStack.entries[undoStack.current - 1];
        if (prev->type == EDIT_DELETE && prev->position == pointerPosition) {
            char deleted = GetCharAt(pointerPosition - 1);

            char* new_text = malloc(prev->length + 2);
            new_text[0] = deleted;
            memcpy(new_text + 1, prev->text, prev->length);
            new_text[prev->length + 1] = '\0';

            free(prev->text);
            prev->text = new_text;
            prev->length++;
            prev->position--;
            prev->cursor_after = pointerPosition - 1;
            
            return true;
        }
    }
    return false;
}

void RemoveCharacterAtPointer() {
    if (pointerPosition == 0) return;
    double current_time = GetTime();

    if (!TryToMergeCharacterRemove(current_time)) {
        char deleted_char = GetCharAt(pointerPosition - 1);
        PushCommand(EDIT_DELETE, pointerPosition - 1, &deleted_char, 1);
    }
    
    if (RemoveCharacter(pointerPosition)) {
        pointerPosition--;
    }

    time_since_last_edit = current_time;
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
    InvalidateLineCache();
}

void InsertStringAtPointer(char* value, size_t len) {
    PushCommand(EDIT_INSERT, pointerPosition, value, len);
    InsertString(pointerPosition, value, len);
    pointerPosition += len;
}

void InsertCharacter(size_t position, char value) {
    InsertString(position, &value, 1);
}

bool TryToMergeCharacterInsert(char value, float current_time) {
    if (undoStack.current > 0 && current_time - time_since_last_edit < 1.0) {
        EditEntry* prev = &undoStack.entries[undoStack.current - 1];

        if (prev->type == EDIT_INSERT && prev->position + prev->length == pointerPosition && value != '\n') {
            char* new_text = realloc(prev->text, prev->length + 2);
            new_text[prev->length] = value;
            new_text[prev->length + 1] = '\0';
            prev->text = new_text;
            prev->length++;
            prev->cursor_after = pointerPosition + 1;
            return true;
        }
    }
    return false;
}

void InsertCharacterAtPointer(char value) {
    double current_time = GetTime();
    if (!TryToMergeCharacterInsert(value, current_time)) {
        char text[2] = {value, '\0'};
        PushCommand(EDIT_INSERT, pointerPosition, text, 1);
    }

    InsertCharacter(pointerPosition, value);
    pointerPosition += 1;

    time_since_last_edit = current_time;
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

void RenderLineBufferWithSelection(char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position) {
    Vector2 draw_length = MeasureTextEx(editor_font, text_buffer, fontSize, 1);
    
    
    if (position.y < selection_start_position.y || position.y > selection_end_position.y) {
        DrawTextEx(editor_font, text_buffer, drawPosition, fontSize, 1, TextColor);
        return;
    }

    char* before_buffer = NULL;
    char* line_buffer = NULL;
    char* after_buffer = NULL;
    Vector2 line_buffer_start = drawPosition;
    int buffer_start = 0;  // Start of selected portion in text_buffer
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
        Vector2 before_buffer_size = MeasureTextEx(editor_font, before_buffer, fontSize, 1);
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
        DrawTextEx(editor_font, before_buffer, drawPosition, fontSize, 1, TextColor);
    }
    
    if (line_buffer != NULL) {
        Vector2 line_buffer_size = MeasureTextEx(editor_font, line_buffer, fontSize, 1);
        DrawRectangle(line_buffer_start.x, line_buffer_start.y, line_buffer_size.x, line_buffer_size.y, TextColor);
        DrawTextEx(editor_font, line_buffer, line_buffer_start, fontSize, 1, BackgroundColor);
    }
    
    if (after_buffer != NULL) {
        Vector2 line_buffer_size = MeasureTextEx(editor_font, line_buffer != NULL ? line_buffer : "", fontSize, 1);
        Vector2 after_start = line_buffer_start;
        after_start.x += line_buffer_size.x;
        DrawTextEx(editor_font, after_buffer, after_start, fontSize, 1, TextColor);
    }

    free(before_buffer);
    free(line_buffer);
    free(after_buffer);
}

void RenderLineBuffer(char* text_buffer, Position position, size_t line_length, Vector2 drawPosition, Position selection_start_position, Position selection_end_position) {
    if (has_selection && position.y >= selection_start_position.y && position.y <= selection_end_position.y) {
        RenderLineBufferWithSelection(text_buffer, position, line_length, drawPosition, selection_start_position, selection_end_position);
    } else {
        DrawTextEx(editor_font, text_buffer, drawPosition, fontSize, 1, TextColor);
    }
}

void RenderLine(int y_line, int xX, int yY, size_t index, Position pointer, Position selection_start_position, Position selection_end_position) {
    Position position = {xX, yY};
    char* temp = NULL;
    size_t line_buffer_length;
    char* line = GenerateLine(index);
    size_t line_length = strlen(line);
    if (pointer.y != index) {
        RenderLineBuffer(line, (Position){0, y_line}, line_length, (Vector2){xX, yY}, selection_start_position, selection_end_position);
        //DrawTextEx(editor_font, line, (Vector2){xX, yY}, fontSize, 1, TextColor); 
    } else {       
        if (temp != NULL) {
            free(temp);
        } 
        temp = calloc(line_length + 1, sizeof(char));
        strncpy(temp, line, pointer.x);
        Vector2 draw_length = MeasureTextEx(editor_font, temp, fontSize, 1);
        RenderLineBuffer(temp, (Position){0, y_line}, pointer.x, (Vector2){xX, yY}, selection_start_position, selection_end_position);
        //DrawTextEx(editor_font, temp, (Vector2){xX, yY}, fontSize, 1, TextColor);
        RenderLineBuffer(line + pointer.x, (Position){pointer.x, y_line}, line_length - pointer.x, (Vector2){xX + draw_length.x + pointerPaddingX * 2 + pointerWidth, yY}, selection_start_position, selection_end_position);
        //DrawTextEx(editor_font, line + pointer.x, (Vector2){xX + draw_length.x + pointerPaddingX * 2 + pointerWidth, yY}, fontSize, 1, TextColor);   
        DrawRectangle(xX + draw_length.x + pointerPaddingX, yY, pointerWidth, fontSize - 2 * pointerPaddingY, TextColor);
    }
    free(line);
    if (temp != NULL) {
        free(temp);
    }
}

size_t GetLineCount()  {
    if (!line_cache.is_valid) {
        RebuildLineCache();
    }
    return line_cache.line_count;
}

Position IndexToPosition(size_t index) {  
    Position out = {0, 0};
    size_t traversed = 0;
    char* work_buffer;
    for (size_t i = 0; i < piece_count && traversed < index; ++i) {
        work_buffer = pieces[i].source == ORIGINAL ? org_buffer : add_buffer;
        
        size_t to_read = index - traversed;
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

void RenderTextBuffer(size_t startX, size_t startY, size_t width, size_t height) {
    Position pointer = GetPointerPosition();
    Position selection_start_position = {0};
    Position selection_end_position = {0};
    if (has_selection) {
        selection_start_position = IndexToPosition(min(selection_start, selection_end));
        selection_end_position = IndexToPosition(max(selection_start, selection_end));
    }
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
        RenderLine(i, startX-offsetX, startY + line_y * fontSize, i, pointer, selection_start_position, selection_end_position);
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

size_t PositionToPointer(Position in) {
    if (!line_cache.is_valid) {
        RebuildLineCache();
    }
    size_t out = 0;

    for (size_t i = 0; i < in.y; ++i) {
        Position line = GetLineByIndex(i);
        out += line.y + 1;
    }

    out += in.x;

    return out;
}

void ExecuteCommand() {
    if (command_args.count < 1) return;
    if (strcmp(command_args.args[0], "find") == 0) {
        if (command_args.count != 2) return;
        size_t line_counter = GetPointerPosition().y + 1;
        size_t line_count = GetLineCount();
        size_t search_length = strlen(command_args.args[1]);
        Position found = {-1, -1};
        for (size_t i = 0; i < line_count; ++i) {
            size_t working_line = (line_counter + i) % line_count;
            char* line = GenerateLine(working_line);
            size_t line_length = strlen(line);
            
            if (line_length < search_length) {
                continue;
            }

            for (size_t j = 0; j < line_length - search_length; ++j) {
                if (strncmp(line + j, command_args.args[1], search_length) == 0) {
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
            pointerPosition = PositionToPointer(found);
        }
                    
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
    } else if (strcmp(command_args.args[0], "quit") == 0) {
        exit_requested = true;
    }
}

void ExecuteInsert(size_t position, const char* text, size_t length) {
    InsertString(position, (char*)text, length);
    pointerPosition = position + length;
}

char* GetTextRange(size_t start, size_t end) {
    size_t length = end - start;
    char* result = malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        result[i] = GetCharAt(start + i);
    }
    result[length] = '\0';
    return result;
}

void ExecuteDelete(size_t position, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        RemoveCharacter(position + 1);
    }
    pointerPosition = position;
}

void RemoveArea(size_t position, size_t length) {
    char* deleted_text = GetTextRange(position, position + length);
    PushCommand(EDIT_DELETE, position, deleted_text, length);
    ExecuteDelete(position, length);
}


void Undo() {
    if (undoStack.current == 0) return;

    undoStack.current--;
    EditEntry* entry = &undoStack.entries[undoStack.current];

    switch (entry->type) 
    {
        case EDIT_INSERT:
            ExecuteDelete(entry->position, entry->length);        
            break;
        case EDIT_DELETE:
            ExecuteInsert(entry->position, entry->text, entry->length);
            break;
    }

    pointerPosition = entry->cursor_before;
    InvalidateLineCache();
}

void Redo() {
    if (undoStack.current >= undoStack.count) return;
    
    EditEntry* entry = &undoStack.entries[undoStack.current];
    
    switch (entry->type) {
        case EDIT_INSERT: {
            ExecuteInsert(entry->position, entry->text, entry->length);
            break;
        }
        case EDIT_DELETE: {
            ExecuteDelete(entry->position, entry->length);
            break;
        }
    }
    
    pointerPosition = entry->cursor_after;
    undoStack.current++;
    InvalidateLineCache();
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
    InitLineCache();
    InitUndoStack();

    pieces[0].source = ORIGINAL;
    pieces[0].start = 0;
    pieces[0].length = strlen(org_buffer);
    piece_count = 1;


    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1200, 700, "Fun Editor");
    SetTargetFPS(60); 
    SetExitKey(KEY_NULL);
    
    editor_font = LoadFontEx("Input.ttf", fontSize, 0, 250);

    while (!WindowShouldClose() && !exit_requested) {
        size_t screenWidth = GetScreenWidth();
        size_t screenHeight = GetScreenHeight();

        if (!is_command_mode) {
            bool shift = IsKeyDown(KEY_LEFT_SHIFT);
            bool control = IsKeyDown(KEY_LEFT_CONTROL);
            int pointer_position_before = pointerPosition;
            if (IsKeyPressed(KEY_LEFT) && pointerPosition > 0) {
                pointerPosition--;
            }
            if (IsKeyPressed(KEY_RIGHT) && pointerPosition <= GetTextSize()) {
                pointerPosition++;
            }
            if (IsKeyPressed(KEY_UP)) {
                if (control) {
                    for (int i = 0; i < 5; i++) {
                        jumpLineUp();
                    }
                } else {
                    jumpLineUp();
                }
            } 
            if (IsKeyPressed(KEY_DOWN)) {
                if (control) {
                    for (int i = 0; i < 5; i++) {
                        jumpLineDown();
                    }
                } else {
                    jumpLineDown();
                }
            } 
            if (!shift && has_selection && pointer_position_before != pointerPosition) {
                has_selection = false;
            }
            if (shift && !has_selection) {
                has_selection = true;
                selection_start = pointer_position_before;
            }
            if (shift) {selection_end = pointerPosition;}

            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
                if (IsKeyDown(KEY_LEFT_SHIFT)) {
                    Redo();
                } else {
                    Undo();
                }
            }


            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (has_selection) {
                    size_t selection_length = abs((int)(selection_end) - (int)(selection_start));
                    RemoveArea(min(selection_start, selection_end), selection_length);
                    has_selection = false;
                } else {
                    RemoveCharacterAtPointer();
                }
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
                
                if (IsKeyPressed(KEY_Q)) {
                    is_command_mode = true;
                    memset(commando_content, 0, MAX_COMMAND_BUFFER);
                    strcpy(commando_content, "quit");
                    command_length = strlen("quit");
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

                if (IsKeyPressed(KEY_ESCAPE)) {
                    is_command_mode = false;
                }
            }
        }

        BeginDrawing(); 
        ClearBackground(BackgroundColor);
        RenderMode();
        RenderTextField(0, mode_padding * 2 + fontSize, screenWidth - 40, screenHeight - (mode_padding * 2 + fontSize * 2 + command_padding * 2));
        RenderCommand(command_padding, screenHeight - command_padding - fontSize);
        EndDrawing();
    }

    CloseWindow();
    free(org_buffer);
    return 0;
}
