#ifndef TEXT_BUFFER_H
#define TEXT_BUFFER_H

#include "../common.h"

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
    size_t saved_current;
} UndoStack;

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
    Position pointer_code_position_cache;
    size_t last_pointer_position_cached;
    bool request_revalidate_pointer_cache;
    bool has_selection;

    UndoStack undo_stack;
} TextBuffer;

LineCache InitLineCache(void);
void ClearLineCache(LineCache* cache);
void RebuildLineCache(TextBuffer* buffer);

UndoStack InitUndoStack(void);
void ClearUndoStack(UndoStack* stack);

void PushCommand(TextBuffer* buffer, EditType type, size_t position, const char* text, size_t length);
bool TryToMergeCharacterInsert(TextBuffer* buffer, char* value, size_t len, float current_time);
bool TryToMergeCharacterRemove(TextBuffer* buffer, float current_time);

void InitTextBuffer(TextBuffer* buffer);
void InitEmptyTextBuffer(TextBuffer* buffer);
void InitTextBufferFromPath(TextBuffer* buffer, const char* path);
void InitPieceBuffer(TextBuffer* buffer);
void ClearTextBuffer(TextBuffer* buffer);

size_t GetTextSize(TextBuffer* buffer);
char GetCharAt(TextBuffer* buffer, size_t position);
size_t GetCodepointAt(TextBuffer* buffer, size_t position, uint32_t* codepoint);
bool IsContinuationByte(TextBuffer* buffer, size_t position);
char* GetTextRangeRaw(TextBuffer* buffer, size_t start, size_t end);
char* GetTextRange(TextBuffer* buffer, size_t start, size_t end);

size_t AppendAddBuffer(TextBuffer* buffer, char* value, size_t len);
void InsertString(TextBuffer* buffer, size_t position, char* value, size_t len);
void ExecuteDelete(TextBuffer* buffer, size_t position, size_t length);
void RemoveArea(TextBuffer* buffer, size_t position, size_t length);
void RemoveSelection(TextBuffer* buffer);

char* FlattenTextBuffer(TextBuffer* buffer, size_t* out_len);

Position GetLinePosition(TextBuffer* buffer, size_t index);
Position GetLineByIndex(TextBuffer* buffer, size_t index);
size_t GetLineCount(TextBuffer* buffer);
char* GenerateLine(TextBuffer* buffer, size_t index);

Position IndexToPosition(TextBuffer* buffer, size_t index);
Position IndexToPositionCodepoint(TextBuffer* buffer, size_t index);
size_t PositionToIndex(TextBuffer* buffer, Position in);
Position GetPointerPosition(TextBuffer* buffer);
Position GetPointerCodePosition(TextBuffer* buffer);
Position GetPointerCodepointPosition(TextBuffer* buffer);
void RevalidatePointerCache(TextBuffer* buffer);

bool IsWordChar(uint32_t c);
bool IsPunct(uint32_t c);
bool IsBufferDirty(TextBuffer* buffer);

#endif
