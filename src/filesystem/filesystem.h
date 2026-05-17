#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "../platform/platform.h"

typedef struct {
    CacheHandle* items;
    uint32_t     count;
    uint32_t     capacity;
} ChildArray;

typedef struct {
    uint32_t generation;
    bool     alive;

    CacheHandle parent;
    ChildArray  children;

    char     name[NAME_MAX_LEN];
    char     name_lower[NAME_MAX_LEN];
    char     rel_path[PATH_MAX_LEN];
    FileType type;

    uint64_t mtime;

    bool is_open;
} FileCacheEntry;

typedef struct {
    CacheHandle parent_handle;
    char abs_path[PATH_MAX_LEN];
    char rel_path[PATH_MAX_LEN];
} ScanFrame;

typedef struct {
    ScanFrame* items;
    size_t count;
    size_t capacity;
} ScanStack;

typedef struct {
    CacheHandle  dir_handle;
    char         abs_path[PATH_MAX_LEN];
    char         rel_path[PATH_MAX_LEN];
    uint64_t     cached_mtime;
} PollFrame;

typedef struct {
    FileSystem*  fs;
    PollFrame*   stack;
    size_t       stack_count;
    size_t       stack_capacity;

    ScanStack    scan_stack;
    FsEntryList  fs_list;

    bool   in_progress;
    bool   request_validation;
    bool   changed;
    size_t dirs_per_frame;
} PollState;

struct FileSystem {
    FileCacheEntry* entries;
    size_t capacity;
    size_t count;

    uint32_t* free_list;
    size_t free_capacity;
    size_t free_count;

    ChildArray root_children;

    CacheHandle* all_handles;
    size_t all_handles_capacity;
    size_t all_handles_count;

    char root_path[PATH_MAX_LEN];
    uint64_t root_mtime;

    double last_scan_time;
    double scan_interval;

    PollState poll_state;

    bool dirty;
};

bool IsIgnoredDir(const char* name);

void InitFsEntryList(FsEntryList* list);
void ClearFsEntryList(FsEntryList* list);
int CompareFsEntry(const void* a, const void* b);
void SortFsEntry(FsEntryList* list);

void InitChildArray(ChildArray* arr);
void ClearChildArray(ChildArray* arr);
void PushChildArray(ChildArray* arr, CacheHandle handle);
void InsertSortedChildArray(FileSystem* system, ChildArray* arr, CacheHandle handle);
uint32_t FindSortedPosChildArray(const FileSystem* system, const ChildArray* arr, const FileCacheEntry* new_entry);
void RemoveChildFromArray(ChildArray* arr, CacheHandle handle);

void InitScaneStack(ScanStack* stack);
void ClearScanStack(ScanStack* stack);
void PushScanStack(ScanStack* stack, CacheHandle parent, const char* abs_path, const char* rel_path);
bool PopScanStack(ScanStack* stack, ScanFrame* out);

void FileSystemEnsureCapacity(FileSystem* system, size_t needed);
CacheHandle FileSystemAlloc(FileSystem* system);
void FileSystemFree(FileSystem* system, CacheHandle handle);
FileCacheEntry* FileSystemGetEntry(const FileSystem* system, CacheHandle handle);
CacheHandle FileSystemCreateEntry(FileSystem* system, CacheHandle parent_handle, const char* rel_dir_path, const FsEntry* fs);
void FileSystemDeleteRecursive(FileSystem* system, CacheHandle handle);
void FileSystemRebuildAllHandles(FileSystem* system);

bool FileSystemBuild(FileSystem* system, const char* root_path);
void ClearFileSystem(FileSystem* system);
FileSystem InitFileSystem(void);

CacheHandle FindChildByName(const FileSystem* system, const ChildArray* children, const char* name);
bool FsEntryListContains(const FsEntryList* list, const char* name);

void FileSystemPollBegin(FileSystem* system);
bool FileSystemPollStep(FileSystem* system, size_t max_dirs);

void PrintTree(const FileSystem* system, const ChildArray* children, int indent);
void PrintFileSystem(const FileSystem* system);

#endif
