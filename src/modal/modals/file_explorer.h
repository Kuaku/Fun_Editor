#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "../modal.h"
#include "../../filesystem/filesystem.h"

typedef struct {
    char** paths;
    size_t count;
    size_t capacity;
} OpenPathSet;

typedef struct {
    CacheHandle handle;
    int depth;
    bool is_dir;
} FlatEntry;

typedef struct {
    FlatEntry* items;
    size_t     count;
    size_t     capacity;
} FlatEntryList;

typedef struct {
    char buffer[256];
    size_t length;
    size_t cursor;
} SearchBar;

typedef struct {
    Editor* editor;
    char base_path[PATH_MAX_LEN];
} CreateFileContext;

typedef struct {
    Editor* editor;

    SearchBar search;
    OpenPathSet open_dirs;

    FlatEntryList all;
    FlatEntryList visible;

    size_t selected_index;
    size_t scroll_offset;

    bool needs_rebuild_all;
    bool needs_rebuild_visible;
} FileExplorerState;

void InitOpenPathSet(OpenPathSet* set);
void ClearOpenPathSet(OpenPathSet* set);
size_t OpenPathSetFind(const OpenPathSet* set, const char* path);
bool OpenPathSetContains(const OpenPathSet* set, const char* path);
void OpenPathSetInsert(OpenPathSet* set, const char* path);
void OpenPathSetRemove(OpenPathSet* set, const char* path);
void OpenPathSetToggle(OpenPathSet* set, const char* path);

void FlatEntryListReset(FlatEntryList* list);
void FlatEntryListClear(FlatEntryList* list);
void FlatEntryListPush(FlatEntryList* list, CacheHandle handle, int depth, bool is_dir);

bool StrContainsCaseInsensitive(const char* haystack, const char* needle);
void EnsurePathVisible(FileSystem* system, OpenPathSet* open_dirs, const char* target_rel_path);

void RebuildAllEntries(FileSystem* system, FlatEntryList* all);
void RebuildVisibleEntries(FileSystem* system, FlatEntryList* all, FlatEntryList* visible, const OpenPathSet* open_dirs, const char* search, size_t search_len);

void FileExplorerSearchInsertChar(FileExplorerState* state, char ch);
void FileExplorerSearchBackspace(FileExplorerState* state);
void FileExplorerSearchClear(FileExplorerState* state);

void FileExplorerRender(Modal* modal, Rect content);
void FileExplorerInput(Modal* modal, RawInput input);
void FileExplorerUpdate(Modal* modal);
void FileExplorerCleanup(void* raw_state);
void FileExplorerResult(Modal* modal, bool confirmed, void* result, void* user_data);

void OnCreateFileOrDirConfirmed(Modal* modal, bool confirmed, void* result, void* user_data);

void RegisterFileExplorerModal(Editor* editor);
void OpenFileExplorerModal(Editor* editor);

#endif
