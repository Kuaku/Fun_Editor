#include "filesystem.h"

static const char* IGNORED_DIRS[] = {
    ".git", "node_modules", ".vs", ".vscode", ".idea",
    "__pycache__", ".cache", ".next", ".nuxt",
    "dist", "build", ".svn", ".hg",
};
#define IGNORED_DIRS_COUNT (sizeof(IGNORED_DIRS) / sizeof(IGNORED_DIRS[0]))

bool IsIgnoredDir(const char* name) {
    for (size_t i = 0; i < IGNORED_DIRS_COUNT; i++) {
#ifdef _WIN32
        if (_stricmp(name, IGNORED_DIRS[i]) == 0) return true;
#else
        if (strcmp(name, IGNORED_DIRS[i]) == 0) return true;
#endif
    }
    return false;
}

void InitFsEntryList(FsEntryList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ClearFsEntryList(FsEntryList* list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int CompareFsEntry(const void* a, const void* b) {
    const FsEntry* ea = (const FsEntry*)a;
    const FsEntry* eb = (const FsEntry*)b;

    if (ea->is_dir != eb->is_dir) {
        return ea->is_dir ? -1 : 1;
    }

    #ifdef _WIN32
        return _stricmp(ea->name, eb->name);
    #else
        return strcasecmp(ea->name, eb->name);
    #endif
}

void SortFsEntry(FsEntryList* list) {
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(FsEntry), CompareFsEntry);
    }
}

void InitChildArray(ChildArray* arr) {
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void ClearChildArray(ChildArray* arr) {
    free(arr->items);
    arr->items = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

void PushChildArray(ChildArray* arr, CacheHandle handle) {
    if (arr->count == arr->capacity) {
        arr->capacity = arr->capacity ? arr->capacity * 2 : 8;
        arr->items = (CacheHandle*)realloc(arr->items, arr->capacity * sizeof(CacheHandle));
    }
    arr->items[arr->count++] = handle;
}

FileCacheEntry* FileSystemGetEntry(const FileSystem* system, CacheHandle handle) {
    if (cache_handle_is_invalid(handle)) return NULL;
    if (handle.index >= system->count) return NULL;

    FileCacheEntry* e = &system->entries[handle.index];
    if (e->generation != handle.generation) return NULL;
    if (!e->alive) return NULL;
    return e;
}

uint32_t FindSortedPosChildArray(const FileSystem* system, const ChildArray* arr, const FileCacheEntry* new_entry) {
    for (uint32_t i = 0; i < arr->count; i++) {
        const FileCacheEntry* existing = FileSystemGetEntry(system, arr->items[i]);
        if (!existing) continue;

        if (new_entry->type == TYPE_DIR && existing->type == TYPE_FILE) {
            return i;
        }
        if (new_entry->type == TYPE_FILE && existing->type == TYPE_DIR) {
            continue;
        }

        #ifdef _WIN32
            if (_stricmp(new_entry->name, existing->name) < 0) return i;
        #else
            if (strcasecmp(new_entry->name, existing->name) < 0) return i;
        #endif
    }
    return arr->count;
}

void InsertSortedChildArray(FileSystem* system, ChildArray* arr, CacheHandle handle) {
    const FileCacheEntry* entry = FileSystemGetEntry(system, handle);
    if (!entry) return;

    uint32_t pos = FindSortedPosChildArray(system, arr, entry);

    if (arr->count == arr->capacity) {
        arr->capacity = arr->capacity ? arr->capacity * 2 : 8;
        arr->items = (CacheHandle*)realloc(arr->items,
                                           arr->capacity * sizeof(CacheHandle));
    }

    if (pos < arr->count) {
        memmove(&arr->items[pos + 1], &arr->items[pos],
                (arr->count - pos) * sizeof(CacheHandle));
    }
    arr->items[pos] = handle;
    arr->count++;
}

void FileSystemEnsureCapacity(FileSystem* system, size_t needed) {
    if (needed <= system->capacity) return;

    size_t new_cap = system->capacity ? system->capacity * 2 : 256;
    while (new_cap < needed) new_cap *= 2;

    system->entries = (FileCacheEntry*)realloc(system->entries, new_cap * sizeof(FileCacheEntry));
    system->capacity = new_cap;
}

CacheHandle FileSystemAlloc(FileSystem* system) {
    uint32_t idx;

    if (system->free_count > 0) {
        idx = system->free_list[--system->free_count];
        system->entries[idx].generation++;
    } else {
        FileSystemEnsureCapacity(system, system->count + 1);
        idx = (uint32_t)system->count++;
        system->entries[idx].generation = 1;
    }

    FileCacheEntry* e = &system->entries[idx];
    e->alive = true;
    e->parent = CACHE_HANDLE_INVALID;
    InitChildArray(&e->children);
    e->name[0] = '\0';
    e->name_lower[0] = '\0';
    e->rel_path[0] = '\0';
    e->type = TYPE_FILE;
    e->mtime = 0;
    e->is_open = false;

    return (CacheHandle){idx, e->generation};
}

void FileSystemFree(FileSystem* system, CacheHandle handle) {
    FileCacheEntry* e = FileSystemGetEntry(system, handle);
    if (!e) return;

    ClearChildArray(&e->children);
    e->alive = false;

    if (system->free_count == system->free_capacity) {
        system->free_capacity = system->free_capacity ? system->free_capacity * 2 : 64;
        system->free_list = (uint32_t*)realloc(
            system->free_list, system->free_capacity * sizeof(uint32_t)
        );
    }
    system->free_list[system->free_count++] = handle.index;
}

void RemoveChildFromArray(ChildArray* arr, CacheHandle handle) {
    for (uint32_t i = 0; i < arr->count; i++) {
        if (cache_handle_eq(arr->items[i], handle)) {
            memmove(&arr->items[i], &arr->items[i + 1],
                (arr->count - i - 1) * sizeof(CacheHandle));
                arr->count--;
                return;
        }
    }
}

void FileSystemDeleteRecursive(FileSystem* system, CacheHandle handle) {
    FileCacheEntry* root_entry = FileSystemGetEntry(system, handle);
    if (!root_entry) return;

    FileCacheEntry* parent = FileSystemGetEntry(system, root_entry->parent);
    if (parent) {
        RemoveChildFromArray(&parent->children, handle);
    } else {
        RemoveChildFromArray(&system->root_children, handle);
    }

    size_t stack_cap = 64;
    size_t stack_count = 0;
    CacheHandle* stack = (CacheHandle*)malloc(stack_cap * sizeof(CacheHandle));

    stack[stack_count++] = handle;

    while (stack_count > 0) {
        CacheHandle current = stack[stack_count - 1];
        FileCacheEntry* entry = FileSystemGetEntry(system, current);

        if (!entry) {
            stack_count--;
            continue;
        }

        if (entry->type == TYPE_DIR && entry->children.count > 0) {
            uint32_t n = entry->children.count;

            while (stack_count + n > stack_cap) {
                stack_cap *= 2;
                stack = (CacheHandle*)realloc(stack, stack_cap * sizeof(CacheHandle));
            }

            for (uint32_t i = 0; i < n; i++) {
                stack[stack_count++] = entry->children.items[i];
            }

            entry->children.count = 0;
        } else {
            stack_count--;
            FileSystemFree(system, current);
        }
    }

    free(stack);
}

void FileSystemRebuildAllHandles(FileSystem* system) {
    system->all_handles_count = 0;

    for (size_t i = 0; i < system->count; i++) {
        if (!system->entries[i].alive) continue;

        if (system->all_handles_count == system->all_handles_capacity) {
            system->all_handles_capacity = system->all_handles_capacity ? system->all_handles_capacity * 2 : 256;
            system->all_handles = (CacheHandle*)realloc(system->all_handles, system->all_handles_capacity * sizeof(CacheHandle));
        }

        system->all_handles[system->all_handles_count++] = (CacheHandle) {(uint32_t)i, system->entries[i].generation};
    }
}

void InitScaneStack(ScanStack* stack) {
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

void ClearScanStack(ScanStack* stack) {
    free(stack->items);
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

void PushScanStack(ScanStack* stack, CacheHandle parent, const char* abs_path, const char* rel_path) {
    if (stack->count == stack->capacity) {
        stack->capacity = stack->capacity ? stack->capacity * 2 : 32;
        stack->items = (ScanFrame*)realloc(stack->items, stack->capacity * sizeof(ScanFrame));
    }

    ScanFrame* f = &stack->items[stack->count++];
    f->parent_handle = parent;
    strncpy(f->abs_path, abs_path, PATH_MAX_LEN - 1);
    f->abs_path[PATH_MAX_LEN - 1] = '\0';
    strncpy(f->rel_path, rel_path, PATH_MAX_LEN - 1);
    f->rel_path[PATH_MAX_LEN - 1] = '\0';
}

bool PopScanStack(ScanStack* stack, ScanFrame* out) {
    if (stack->count == 0) return false;
    *out = stack->items[--stack->count];
    return true;
}

CacheHandle FileSystemCreateEntry(FileSystem* system, CacheHandle parent_handle, const char* rel_dir_path, const FsEntry* fs) {
    size_t estimated_len;
    if (rel_dir_path[0] == '\0') {
        estimated_len = strlen(fs->name);
    } else {
        estimated_len = strlen(rel_dir_path) + 1 + strlen(fs->name);
    }

    if (estimated_len >= PATH_MAX_LEN) {
        fprintf(stderr, "Warning: Relative path too long, skipping: %s\n", fs->name);
        return CACHE_HANDLE_INVALID;
    }

    CacheHandle handle = FileSystemAlloc(system);
    FileCacheEntry* entry = FileSystemGetEntry(system, handle);
    if (!entry) return CACHE_HANDLE_INVALID;

    strncpy(entry->name, fs->name, NAME_MAX_LEN - 1);
    entry->name[NAME_MAX_LEN - 1] = '\0';
    StrToLower(entry->name, entry->name_lower, NAME_MAX_LEN);

    if (rel_dir_path[0] == '\0') {
        strncpy(entry->rel_path, fs->name, PATH_MAX_LEN - 1);
    } else {
#ifdef _WIN32
        snprintf(entry->rel_path, PATH_MAX_LEN, "%s\\%s", rel_dir_path, fs->name);
#else
        snprintf(entry->rel_path, PATH_MAX_LEN, "%s/%s", rel_dir_path, fs->name);
#endif
    }
    entry->rel_path[PATH_MAX_LEN - 1] = '\0';

    entry->type = fs->is_dir ? TYPE_DIR : TYPE_FILE;
    entry->mtime = fs->mtime;
    entry->parent = parent_handle;
    entry->is_open = false;
    return handle;
}

void ClearFileSystem(FileSystem* system) {
    for (size_t i = 0; i < system->count; i++) {
        if (system->entries[i].alive) {
            ClearChildArray(&system->entries[i].children);
        }
    }

    free(system->entries);
    system->entries = 0;
    system->capacity = 0;
    system->count = 0;

    free(system->free_list);
    system->free_list = NULL;
    system->free_capacity = 0;
    system->free_count = 0;

    ClearChildArray(&system->root_children);

    free(system->all_handles);
    system->all_handles = NULL;
    system->all_handles_capacity = 0;
    system->all_handles_count = 0;

    system->root_path[0] = '\0';
    system->root_mtime   = 0;
    system->dirty         = false;
}

bool FileSystemBuild(FileSystem* system, const char* root_path) {
    ClearFileSystem(system);

    strncpy(system->root_path, root_path, PATH_MAX_LEN - 1);
    system->root_path[PATH_MAX_LEN - 1] = '\0';

    system->root_mtime = PlatformGetMTime(root_path);
    if (system->root_mtime == 0) return false;

    FsEntryList fs_list;
    InitFsEntryList(&fs_list);

    ScanStack stack;
    InitScaneStack(&stack);

    PushScanStack(&stack, CACHE_HANDLE_INVALID, root_path, "");

    ScanFrame frame;
    while (PopScanStack(&stack, &frame)) {

        if (!PlatformReadDirectory(frame.abs_path, &fs_list)) continue;
        SortFsEntry(&fs_list);

        for (size_t i = 0; i < fs_list.count; i++) {
            FsEntry* fs = &fs_list.items[i];

            if (fs->is_dir && IsIgnoredDir(fs->name)) continue;

            CacheHandle handle = FileSystemCreateEntry(system, frame.parent_handle, frame.rel_path, fs);
            if (cache_handle_is_invalid(handle)) continue;

            ChildArray* parent_children;
            if (cache_handle_is_invalid(frame.parent_handle)) {
                parent_children = &system->root_children;
            } else {
                FileCacheEntry* parent_entry = FileSystemGetEntry(system, frame.parent_handle);
                if (!parent_entry) continue;
                parent_children = &parent_entry->children;
            }

            PushChildArray(parent_children, handle);

            if (fs->is_dir) {
                char abs_child[PATH_MAX_LEN];
                if (PlatformJoinPath(abs_child, sizeof(abs_child), frame.abs_path, fs->name)) {
                    FileCacheEntry* entry = FileSystemGetEntry(system, handle);
                    if (entry) {
                        PushScanStack(&stack, handle, abs_child, entry->rel_path);
                    }
                } else {
                    fprintf(stderr, "Skipping directory (path too long): %s\n", fs->name);
                }
            }
        }
    }

    ClearFsEntryList(&fs_list);
    ClearScanStack(&stack);

    FileSystemRebuildAllHandles(system);
    system->dirty = true;

    return true;
}

FileSystem InitFileSystem() {
    FileSystem system = {0};
    system.scan_interval = INITIAL_SCAN_INTERVAL;
    InitChildArray(&system.root_children);
    return system;
}

CacheHandle FindChildByName(const FileSystem* system, const ChildArray* children, const char* name) {
    for (uint32_t i = 0; i < children->count; i++) {
        const FileCacheEntry* e = FileSystemGetEntry(system, children->items[i]);
        if (!e) continue;
#ifdef _WIN32
        if (_stricmp(e->name, name) == 0) return children->items[i];
#else
        if (strcmp(e->name, name) == 0) return children->items[i];
#endif
    }
    return CACHE_HANDLE_INVALID;
}

bool FsEntryListContains(const FsEntryList* list, const char* name) {
    for (size_t i = 0; i < list->count; i++) {
#ifdef _WIN32
        if (_stricmp(list->items[i].name, name) == 0) return true;
#else
        if (strcmp(list->items[i].name, name) == 0) return true;
#endif
    }
    return false;
}

void FileSystemPollBegin(FileSystem* system) {
    PollState* ps = &system->poll_state;
    ps->in_progress = true;
    ps->changed = false;
    ps->stack_count = 0;

    if (ps->stack_count >= ps->stack_capacity) {
        ps->stack_capacity = ps->stack_capacity ? ps->stack_capacity * 2 : 32;
        ps->stack = realloc(ps->stack, ps->stack_capacity * sizeof(PollFrame));
    }
    PollFrame* f = &ps->stack[ps->stack_count++];
    f->dir_handle = CACHE_HANDLE_INVALID;
    strncpy(f->abs_path, system->root_path, PATH_MAX_LEN - 1);
    f->abs_path[PATH_MAX_LEN - 1] = '\0';
    f->rel_path[0] = '\0';
    f->cached_mtime = system->root_mtime;
}

bool FileSystemPollStep(FileSystem* system, size_t max_dirs) {
    PollState* ps = &system->poll_state;
    size_t dirs_processed = 0;

    while (ps->stack_count > 0 && dirs_processed < max_dirs) {
        PollFrame frame = ps->stack[--ps->stack_count];

        ChildArray* dir_children;
        if (cache_handle_is_invalid(frame.dir_handle)) {
            dir_children = &system->root_children;
        } else {
            FileCacheEntry* e = FileSystemGetEntry(system, frame.dir_handle);
            if (!e) continue;
            dir_children = &e->children;
        }

        dirs_processed++;

        uint64_t current_mtime = PlatformGetMTime(frame.abs_path);

        if (current_mtime != frame.cached_mtime) {
            FileCacheEntry* e = FileSystemGetEntry(system, frame.dir_handle);
            if (e) e->mtime = current_mtime;

            if (PlatformReadDirectory(frame.abs_path, &ps->fs_list)) {
                for (int32_t i = (int32_t)dir_children->count - 1; i >= 0; i--) {
                    CacheHandle child_h = dir_children->items[i];
                    FileCacheEntry* child = FileSystemGetEntry(system, child_h);
                    if (!child) continue;

                    if (!FsEntryListContains(&ps->fs_list, child->name)) {
                        FileSystemDeleteRecursive(system, child_h);
                        ps->changed = true;
                    }
                }

                for (size_t i = 0; i < ps->fs_list.count; i++) {
                    FsEntry* fs = &ps->fs_list.items[i];

                    if (fs->is_dir && IsIgnoredDir(fs->name)) continue;

                    CacheHandle existing = FindChildByName(system, dir_children, fs->name);

                    if (cache_handle_is_invalid(existing)) {
                        CacheHandle handle = FileSystemCreateEntry(system, frame.dir_handle, frame.rel_path, fs);
                        if (cache_handle_is_invalid(handle)) continue;

                        ChildArray* dir_children;
                        if (cache_handle_is_invalid(frame.dir_handle)) {
                            dir_children = &system->root_children;
                        } else {
                            FileCacheEntry* dir_entry = FileSystemGetEntry(system, frame.dir_handle);
                            if (!dir_entry) continue;
                            dir_children = &dir_entry->children;
                        }

                        InsertSortedChildArray(system, dir_children, handle);

                        if (fs->is_dir) {
                            char abs_child[PATH_MAX_LEN];
                            if (!PlatformJoinPath(abs_child, sizeof(abs_child), frame.abs_path, fs->name)) {
                                continue;
                            }
                            FileCacheEntry* entry = FileSystemGetEntry(system, handle);
                            if (entry) {
                                PushScanStack(&ps->scan_stack, handle, abs_child, entry->rel_path);
                            }
                        }

                        ps->changed = true;
                    } else {
                        FileCacheEntry* e = FileSystemGetEntry(system, existing);
                        if (e) {
                            FileType fs_type = fs->is_dir ? TYPE_DIR : TYPE_FILE;
                            if (e->type != fs_type) {
                                FileSystemDeleteRecursive(system, existing);

                                CacheHandle handle = FileSystemCreateEntry(system, frame.dir_handle, frame.rel_path, fs);
                                if (cache_handle_is_invalid(handle)) continue;

                                ChildArray* dir_children;
                                if (cache_handle_is_invalid(frame.dir_handle)) {
                                    dir_children = &system->root_children;
                                } else {
                                    FileCacheEntry* dir_entry = FileSystemGetEntry(system, frame.dir_handle);
                                    if (!dir_entry) continue;
                                    dir_children = &dir_entry->children;
                                }

                                InsertSortedChildArray(system, dir_children, handle);

                                if (fs->is_dir) {
                                    char abs_child[PATH_MAX_LEN];
                                    if (!PlatformJoinPath(abs_child, sizeof(abs_child), frame.abs_path, fs->name)) {
                                        continue;
                                    }
                                    FileCacheEntry* entry = FileSystemGetEntry(system, handle);
                                    if (entry) {
                                        PushScanStack(&ps->scan_stack, handle, abs_child, entry->rel_path);
                                    }
                                }

                                ps->changed = true;
                            } else {
                                e->mtime = fs->mtime;
                            }
                        }
                    }
                }
            }
        }

        {
            uint32_t n = dir_children->count;
            for (uint32_t i = 0; i < n; i++) {
                CacheHandle child_h = dir_children->items[i];
                FileCacheEntry* child = FileSystemGetEntry(system, child_h);
                if (!child || child->type != TYPE_DIR) continue;
                if (IsIgnoredDir(child->name)) continue;

                if (ps->stack_count == ps->stack_capacity) {
                    ps->stack_capacity = ps->stack_capacity ? ps->stack_capacity * 2 : 32;
                    ps->stack = (PollFrame*)realloc(ps->stack, ps->stack_capacity * sizeof(PollFrame));
                }

                PollFrame* pf = &ps->stack[ps->stack_count++];
                pf->dir_handle = child_h;
                if (!PlatformJoinPath(pf->abs_path, sizeof(pf->abs_path), frame.abs_path, child->name)) {
                    ps->stack_count--;
                    continue;
                }
                strncpy(pf->rel_path, child->rel_path, PATH_MAX_LEN - 1);
                pf->rel_path[PATH_MAX_LEN - 1] = '\0';
                pf->cached_mtime = child->mtime;
            }
        }
    }

    {
        ScanFrame sf;
        while (dirs_processed < max_dirs && PopScanStack(&ps->scan_stack, &sf)) {
            dirs_processed++;

            if (!PlatformReadDirectory(sf.abs_path, &ps->fs_list)) continue;
            SortFsEntry(&ps->fs_list);

            for (size_t i = 0; i < ps->fs_list.count; i++) {
                FsEntry* fs = &ps->fs_list.items[i];

                if (fs->is_dir && IsIgnoredDir(fs->name)) continue;

                CacheHandle handle = FileSystemCreateEntry(system, sf.parent_handle, sf.rel_path, fs);
                if (cache_handle_is_invalid(handle)) continue;

                ChildArray* parent_children;
                if (cache_handle_is_invalid(sf.parent_handle)) {
                    parent_children = &system->root_children;
                } else {
                    FileCacheEntry* parent_entry = FileSystemGetEntry(system, sf.parent_handle);
                    if (!parent_entry) continue;
                    parent_children = &parent_entry->children;
                }

                PushChildArray(parent_children, handle);

                if (fs->is_dir) {
                    char abs_child[PATH_MAX_LEN];
                    if (!PlatformJoinPath(abs_child, sizeof(abs_child), sf.abs_path, fs->name)) {
                        continue;
                    }
                    FileCacheEntry* entry = FileSystemGetEntry(system, handle);
                    if (entry) {
                        PushScanStack(&ps->scan_stack, handle, abs_child, entry->rel_path);
                    }
                }
            }
        }
    }

    if (ps->stack_count == 0 && ps->scan_stack.count == 0) {
        if (ps->changed) {
            FileSystemRebuildAllHandles(system);
            system->dirty = true;
        }
        ps->in_progress = false;
        return true;
    }
    return false;
}

void PrintTree(const FileSystem* system, const ChildArray* children, int indent) {
    for (uint32_t i = 0; i < children->count; i++) {
        FileCacheEntry* e = FileSystemGetEntry(system, children->items[i]);
        if (!e) continue;
        for (int j = 0; j < indent; j++) printf("  ");
        if (e->type == TYPE_DIR) {
            printf("[DIR]  %s  (rel: %s)\n", e->name, e->rel_path);
            PrintTree(system, &e->children, indent + 1);
        } else {
            printf("[FILE] %s  (rel: %s)\n", e->name, e->rel_path);
        }
    }
}

void PrintFileSystem(const FileSystem* system) {
    printf("\n=== FileSystem ===\n");
    printf("Root: %s\n", system->root_path);
    printf("Alive: %zu / %zu slots, Free: %zu\n",
           system->all_handles_count, system->count, system->free_count);
    printf("Dirty: %s\n\n", system->dirty ? "YES" : "NO");
    PrintTree(system, &system->root_children, 0);
    printf("\n");
}
