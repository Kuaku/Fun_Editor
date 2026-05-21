#include "text_buffer.h"
#include "../platform/platform.h"
#include "../utils/utf8.h"

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

static void ClearEditEntry(EditEntry* entry) {
    if (!entry) return;

    if (entry->text) {
        free(entry->text);
        entry->text = NULL;
        entry->length = 0;
    }
}

UndoStack InitUndoStack() {
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

size_t GetTextSize(TextBuffer* buffer) {
    size_t out = 0;
    for (size_t i = 0; i < buffer->piece_count; ++i) {
        out += buffer->pieces[i].length;
    }
    return out;
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

size_t GetCodepointAt(TextBuffer* buffer, size_t position, uint32_t* codepoint) {
    char utf8_buffer[4];
    utf8_buffer[0] = GetCharAt(buffer, position);
    size_t utf8_length = utf8_get_length(utf8_buffer);
    for (size_t i = 1; i < utf8_length; i++) {
        utf8_buffer[i] = GetCharAt(buffer, position + i);
    }
    size_t result = utf8_decode(utf8_buffer, codepoint);
    return result;
}

bool IsContinuationByte(TextBuffer* buffer, size_t position) {
    return utf8_is_continuation(GetCharAt(buffer, position));
}

char* GetTextRangeRaw(TextBuffer* buffer, size_t start, size_t end) {
    size_t length = end - start;
    char* result = malloc(length);
    for (size_t i = 0; i < length; i++) {
        result[i] = GetCharAt(buffer, start + i);
    }
    return result;
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

bool TryToMergeCharacterRemove(TextBuffer* buffer, float current_time) {
    UndoStack* stack = &buffer->undo_stack;
    if (stack->current > 0 && current_time - buffer->time_since_last_edit < 1.0) {
        EditEntry* prev = &stack->entries[stack->current - 1];
        if (prev->type == EDIT_DELETE && prev->position == buffer->pointer_position) {
            size_t start_byte_position = buffer->pointer_position - 1;
            while (start_byte_position > 0 && IsContinuationByte(buffer, start_byte_position)) {
                start_byte_position--;
            }
            char* utf8_buffer = GetTextRange(buffer, start_byte_position, buffer->pointer_position);
            size_t utf8_length = buffer->pointer_position - start_byte_position;
            

            char* new_text = malloc(prev->length + utf8_length + 1);
            memcpy(new_text, utf8_buffer, utf8_length);
            memcpy(new_text + utf8_length, prev->text, prev->length);
            new_text[utf8_length + prev->length] = '\0';

            free(utf8_buffer);
            free(prev->text);
            prev->text = new_text;
            prev->length += utf8_length;
            prev->position -= utf8_length;
            prev->cursor_after = buffer->pointer_position - utf8_length;

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

void InitTextBuffer(TextBuffer* buffer) {
    buffer->add_buffer = calloc(INITIAL_ADD_BUFFER_CAPACITY, sizeof(char));
    buffer->add_buffer_capacity = INITIAL_ADD_BUFFER_CAPACITY;
    buffer->add_buffer_count = 0;

    buffer->line_cache = InitLineCache();

    buffer->line_anchor = 0;
    buffer->offset_x = 0;
    buffer->pointer_position = 0;
    buffer->pointer_position_cache = (Position){0, 0};
    buffer->pointer_code_position_cache = (Position){0, 0};
    buffer->last_pointer_position_cached = 0;
    buffer->request_revalidate_pointer_cache = false;
    buffer->time_since_last_edit = 0;

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

void ExecuteDelete(TextBuffer* buffer, size_t position, size_t length) {
    size_t del_start = position;
    size_t del_end = position + length;

    Piece* new_pieces = malloc((buffer->piece_count + 2) * sizeof(Piece));
    int new_count = 0;
    size_t current_pos = 0;

    for (size_t i = 0; i < buffer->piece_count; ++i) {
        Piece p = buffer->pieces[i];
        size_t piece_start = current_pos;
        size_t piece_end = current_pos + p.length;

        if (piece_end <= del_start || piece_start >= del_end) {
            new_pieces[new_count++] = p;
        } else {
            if (piece_start < del_start) {
                Piece left = p;
                left.length = del_start - piece_start;
                new_pieces[new_count++] = left;
            }
            if (piece_end > del_end) {
                Piece right = p;
                size_t skip = del_end - piece_start;
                right.start += skip;
                right.length -= skip;
                new_pieces[new_count++] = right;
            }
        }

        current_pos += p.length;
    }

    while (new_count > buffer->piece_capacity) {
        buffer->piece_capacity = new_count * 2;
        buffer->pieces = realloc(buffer->pieces, buffer->piece_capacity * sizeof(Piece));
    }
    memcpy(buffer->pieces, new_pieces, sizeof(Piece) * new_count);
    buffer->piece_count = new_count;
    free(new_pieces);
    buffer->line_cache.is_valid = false;
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

char* FlattenTextBuffer(TextBuffer* buffer, size_t* out_len) {
    size_t total = GetTextSize(buffer);
    char* result = malloc(total + 1);
    if (!result) return NULL;

    size_t written = 0;
    for (size_t i = 0; i < buffer->piece_count; i++) {
        char* src = buffer->pieces[i].source == ORIGINAL ? buffer->org_buffer : buffer->add_buffer;

        memcpy(result + written, src + buffer->pieces[i].start, buffer->pieces[i].length);
        written += buffer->pieces[i].length;
    }

    result[written] = '\0';
    if (out_len) *out_len = written;
    return result;
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

size_t GetLineCount(TextBuffer* buffer) {
    if (!buffer->line_cache.is_valid) {
        RebuildLineCache(buffer);
    }
    return buffer->line_cache.line_count;
}

char* GenerateLine(TextBuffer* buffer, size_t index) {
    char* line;
    Position line_position = GetLineByIndex(buffer, index);
    line = calloc(line_position.y + 1, sizeof(char));

    size_t start_pos = line_position.x;
    size_t end_pos = start_pos + line_position.y;

    size_t traversed = 0;
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

Position IndexToPositionCodepoint(TextBuffer* buffer, size_t index) {
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
            } else if (!utf8_is_continuation(work_buffer[buffer->pieces[i].start + j])) {
                out.x++;
            }
        }
        traversed += to_read;
    }
    return out;
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

Position GetPointerPosition(TextBuffer* buffer) {
    if (buffer->request_revalidate_pointer_cache || buffer->pointer_position != buffer->last_pointer_position_cached) RevalidatePointerCache(buffer);
    
    return buffer->pointer_position_cache;
}

Position GetPointerCodePosition(TextBuffer* buffer) {
    if (buffer->request_revalidate_pointer_cache || buffer->pointer_position != buffer->last_pointer_position_cached) RevalidatePointerCache(buffer);
    
    return buffer->pointer_code_position_cache;
}


void RevalidatePointerCache(TextBuffer* buffer) {
    Position out = IndexToPosition(buffer, buffer->pointer_position);
    Position codepoint_position = IndexToPositionCodepoint(buffer, buffer->pointer_position);

    buffer->request_revalidate_pointer_cache = false;
    buffer->pointer_position_cache = out;
    buffer->pointer_code_position_cache = codepoint_position;
    buffer->last_pointer_position_cached = buffer->pointer_position;
}

Position GetPointerCodepointPosition(TextBuffer* buffer) {
    Position pointer = GetPointerPosition(buffer);
    Position line = GetLineByIndex(buffer, pointer.y);

    size_t codepoint_x = 0;
    for (size_t i = 0; i < pointer.x; i++) {
        if (!IsContinuationByte(buffer, line.x + i)) {
            codepoint_x++;
        }
    }

    return (Position){codepoint_x, pointer.y};
}

bool IsWordChar(uint32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c > 127;
}

bool IsPunct(uint32_t c) {
    return c && !IsWordChar(c) && c != ' ' && c != '\t' && c != '\n';
}
