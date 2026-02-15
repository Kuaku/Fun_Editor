#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN 
    #define NOGDI             
    #define NOUSER               
    #include <windows.h>
    #undef WIN32_LEAN_AND_MEAN
    #undef NOGDI
    #undef NOUSER
    
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

#ifndef _WIN32
    #define min(a, b) ((a) < (b) ? (a) : (b))
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif

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

typedef void (*Layout)(void* self);

typedef struct {
    Vector2 position;
    Vector2 size;
    Vector2 wanted_size;
    Vector2 padding;
    Color background_color;
    Layout* layouts;
    int layout_count;
    void (*render)(void* self);
    void (*input)(void* self);
    void* state;
} Modal;

typedef enum {
    TYPE_FILE,
    TYPE_DIR
} NodeType;

typedef struct {
    NodeType type;

    char* path;
    size_t path_length;
    char* name;
    size_t name_length;
    int* children;
    size_t children_count;
} Node;

typedef struct {
    char* path;
    char* name;
    size_t parent_index;
} DirWorkItem;

typedef struct {
    char* path;
    size_t index;
} PendingDir;

typedef struct {
    Node* nodes;
    size_t count;
    size_t capacity;
    size_t root_index;
} FileTree;

typedef struct {
    FileTree* active;     
    FileTree* building;   
    
    double last_scan_time;
    double scan_interval;
    
    bool rebuild_requested;
    bool rebuild_in_progress;
    
    PendingDir* pending_dirs;
    size_t pending_count;
    size_t pending_capacity;
    size_t dirs_per_frame;  
} FileCacheManager;

typedef struct {
    size_t selected_index;
    size_t scroll_offset;
    size_t expanded_dirs[256];
    size_t expanded_count;
} FileBrowserState;

char* root_path;

FileCacheManager cache_manager = {
    .active = NULL,
    .building = NULL,
    .last_scan_time = 0,
    .scan_interval = 3.0,
    .rebuild_requested = false,
    .rebuild_in_progress = false,
    .pending_dirs = NULL,
    .pending_count = 0,
    .pending_capacity = 0,
    .dirs_per_frame = 50
};

#define MAX_PIECES 1024
#define MAX_ADD_BUFFER 4096
#define MAX_COMMAND_BUFFER 4096

#define BackgroundColor (Color){32, 35, 41, 255}
#define TextColor       WHITE
#define ModeColor       WHITE
#define CommandColor    WHITE
#define LineNumberColor YELLOW

typedef struct {
    char* root_dir;

    TextBuffer* text_buffers;
    size_t text_buffers_capacity;
    size_t text_buffers_count;


} EditorState;

typedef struct {
    char* file_path;

    char* org_buffer;
    char* add_buffer;
    size_t add_buffer_capacity;
    size_t add_buffer_count;

    Piece* pieces;
    size_t piece_capacity;
    size_t piece_count;

    LineCache line_cache;

    size_t line_anchor;

    size_t pointerPosition;
} TextBuffer;

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

Modal* modal = NULL;


FileTree* GetFileTree() {
    return cache_manager.active;
}

bool IsDirExpanded(FileBrowserState* state, size_t node_index) {
    for (size_t i = 0; i < state->expanded_count; i++) {
        if (state->expanded_dirs[i] == node_index) return true;
    }
    return false;
}

// TODO: Rework to use Arena
void ToggleDirExpanded(FileBrowserState* state, size_t node_index) {
    for (size_t i = 0; i < state->expanded_count; i++) {
        if (state->expanded_dirs[i] == node_index) {
            memmove(&state->expanded_dirs[i], &state->expanded_dirs[i + 1],
                    sizeof(size_t) * (state->expanded_count - i - 1));
            state->expanded_count--;
            return;
        }
    }
    if (state->expanded_count < 256) {
        state->expanded_dirs[state->expanded_count++] = node_index;
    }
}

void RenderFileNode(Modal* modal, FileTree* tree, size_t node_index, FileBrowserState* state, float x, float* y, float indent, float item_height, float visible_top, float visible_bottom) {
    if (!tree || !tree->nodes || node_index >= tree->count) {
        return;
    }
    Node* node = &tree->nodes[node_index];

    if (*y + item_height < visible_top || *y > visible_bottom) {
        *y += item_height;
        if (node->type == TYPE_DIR && IsDirExpanded(state, node_index)) {
            for (size_t i = 0; i < node->children_count; i++) {
                RenderFileNode(modal, tree, node->children[i], state, x, y, indent + 20, item_height, visible_top, visible_bottom);
            }
        }
        return;
    }

    bool is_selected = (state->selected_index == node_index);
    if (is_selected) {
        DrawRectangle(x, *y, modal->size.x, item_height, (Color){60, 60, 80, 255});
    }

    const char* icon = (node->type == TYPE_DIR) 
        ? (IsDirExpanded(state, node_index) ? "v " : "> ")
        : "  ";

    char display[256];
    snprintf(display, sizeof(display), "%s%s", icon, node->name);
    DrawTextEx(editor_font, display, (Vector2){x + indent, *y}, fontSize, 1, TextColor);
    *y += item_height;

    if (node->type == TYPE_DIR && IsDirExpanded(state, node_index)) {
        for (size_t i = 0; i < node->children_count; i++) {
            RenderFileNode(modal, tree, node->children[i], state, x, y, indent+20, item_height, visible_top, visible_bottom);
        }
    }
}

size_t CollectVisibleNodes(FileTree* tree, FileBrowserState* state, size_t node_index, 
                           size_t* visible, size_t count, size_t max_count) {

    if (!tree || !visible || node_index >= tree->count) {
        return count;
    }
    if (count >= max_count) return count;
    
    visible[count++] = node_index;
    
    Node* node = &tree->nodes[node_index];
    if (node->type == TYPE_DIR && IsDirExpanded(state, node_index)) {
        for (size_t i = 0; i < node->children_count; i++) {
            count = CollectVisibleNodes(tree, state, node->children[i], 
                                        visible, count, max_count);
        }
    }
    
    return count;
}

void MoveSelectionUp(FileBrowserState* state, FileTree* tree) {
    size_t visible[4096];
    size_t visible_count = CollectVisibleNodes(tree, state, tree->root_index, visible, 0, 4096);
    
    for (size_t i = 1; i < visible_count; i++) {
        if (visible[i] == state->selected_index) {
            state->selected_index = visible[i - 1];
            return;
        }
    }
}

void MoveSelectionDown(FileBrowserState* state, FileTree* tree) {
    size_t visible[4096];
    size_t visible_count = CollectVisibleNodes(tree, state, tree->root_index, visible, 0, 4096);
    
    for (size_t i = 0; i < visible_count - 1; i++) {
        if (visible[i] == state->selected_index) {
            state->selected_index = visible[i + 1];
            return;
        }
    }
}

int GetVisibleIndex(FileTree* tree, FileBrowserState* state, size_t target_index) {
    size_t visible[4096];
    size_t visible_count = CollectVisibleNodes(tree, state, tree->root_index, visible, 0, 4096);
    
    for (size_t i = 0; i < visible_count; i++) {
        if (visible[i] == target_index) {
            return (int)i;
        }
    }
    return -1;
}

void EnsureSelectionVisible(Modal* modal, FileBrowserState* state, FileTree* tree) {
    int visible_index = GetVisibleIndex(tree, state, state->selected_index);
    if (visible_index < 0) return;
    
    float item_height = fontSize + 4;
    float selection_y = visible_index * item_height;
    float visible_height = modal->size.y - 20;
    
    if (selection_y < state->scroll_offset) {
        state->scroll_offset = (size_t)selection_y;
    }
    
    if (selection_y + item_height > state->scroll_offset + visible_height) {
        state->scroll_offset = (size_t)(selection_y + item_height - visible_height);
    }
}

void InputFileBrowser(void* modal_ptr) {
    Modal* self = (Modal*)modal_ptr;
    FileBrowserState* state = (FileBrowserState*)self->state;
    FileTree* tree = GetFileTree();

    if (!tree) return;
    
    if (IsKeyPressed(KEY_UP)) {
        MoveSelectionUp(state, tree);
        EnsureSelectionVisible(self, state, tree);
    }
    
    if (IsKeyPressed(KEY_DOWN)) {
        MoveSelectionDown(state, tree);
        EnsureSelectionVisible(self, state, tree);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        Node* node = &tree->nodes[state->selected_index];
        if (node->type == TYPE_DIR) {
            ToggleDirExpanded(state, state->selected_index);
            EnsureSelectionVisible(self, state, tree);
        } else {
            // TODO: Open file
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) ) {
        modal = NULL;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_SPACE)) {
        modal = NULL;
    }
}

void RenderFileBrowser(void* modal_ptr) {
    Modal* self = (Modal*)modal_ptr;
    if (!self || !self->state) {
        return;
    }
    FileBrowserState* state = (FileBrowserState*)self->state;

    FileTree* tree = GetFileTree();

    if (!tree || tree->count == 0) {
        const char* msg = cache_manager.rebuild_in_progress 
            ? "Loading..." 
            : "No files";
        Vector2 size = MeasureTextEx(editor_font, msg, fontSize, 1);
        DrawTextEx(editor_font, msg, 
            (Vector2){
                self->position.x + self->size.x/2 - size.x/2,
                self->position.y + self->size.y/2 - size.y/2
            }, 
            fontSize, 1, TextColor);
        return;
    }

    if (tree->root_index >= tree->count) {
        return;
    }

    float y = self->position.y + 10 - state->scroll_offset;
    float visible_top = self->position.y;
    float visible_bottom = self->position.y + self->size.y;

    RenderFileNode(self, tree, tree->root_index, state, self->position.x, &y, 0, fontSize + 4, visible_top, visible_bottom);
}

FileTree* CreateFileTree() {
    FileTree* tree = calloc(1, sizeof(FileTree));
    tree->capacity = 256;
    tree->nodes = calloc(tree->capacity, sizeof(Node));
    tree->count = 0;
    return tree;
}

void FreeFileTree(FileTree* tree) {
    if (!tree) return;
    for (size_t i = 0; i < tree->count; i++) {
        free(tree->nodes[i].path);
        free(tree->nodes[i].name);
        free(tree->nodes[i].children);
    }
    free(tree->nodes);
    free(tree);
}

size_t AddNodeToTree(FileTree* tree, NodeType type, const char* path, const char* name) {
    if (tree->count >= tree->capacity) {
        size_t new_capacity = tree->capacity * 2;
        Node* new_nodes = realloc(tree->nodes, sizeof(Node) * new_capacity);

        if (!new_nodes) {
            return SIZE_MAX;
        }
        tree->nodes = new_nodes;
        tree->capacity = new_capacity;
    }
    
    size_t index = tree->count++;
    Node* node = &tree->nodes[index];
    node->type = type;
    node->path = strdup(path);
    node->name = strdup(name);

    if (!node->path || !node->name) {
        free(node->path);
        free(node->name);
        tree->count--;
        return SIZE_MAX;
    }

    node->path_length = strlen(path);
    node->name_length = strlen(name);
    node->children = NULL;
    node->children_count = 0;
    
    return index;
}

void AddPendingDir(const char* path, size_t node_index) {
    if (node_index == SIZE_MAX) return;

    if (cache_manager.pending_count >= cache_manager.pending_capacity) {
        size_t new_capacity = cache_manager.pending_capacity == 0 
            ? 64 : cache_manager.pending_capacity * 2;
        PendingDir* new_dirs = realloc(cache_manager.pending_dirs,
            sizeof(PendingDir) * new_capacity);
        if (!new_dirs) {
            return;
        }
        cache_manager.pending_dirs = new_dirs;
        cache_manager.pending_capacity = new_capacity;
    }

    char* path_copy = strdup(path);
    if (!path_copy) {
        return;
    }
    
    cache_manager.pending_dirs[cache_manager.pending_count++] = (PendingDir){
        .path = path_copy,
        .index = node_index
    };
}

void AddChildToTreeNode(FileTree* tree, size_t parent_index, size_t child_index) {
    if (parent_index >= tree->count || child_index == SIZE_MAX) {
        return;
    }

    Node* parent = &tree->nodes[parent_index];
    size_t current_capacity = (parent->children_count == 0) ? 0 : 
        ((parent->children_count - 1) / 16 + 1) * 16;
    
    if (parent->children_count >= current_capacity) {
        size_t new_capacity = current_capacity + 16;
        int* new_children = realloc(parent->children, sizeof(int) * new_capacity);
        if (!new_children) {
            return;
        }
        parent->children = new_children;
    }

    parent->children[parent->children_count++] = (int)child_index;
}

char* JoinPath(const char* base, const char* name) {
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    char* result = malloc(base_len + name_len + 2);
    
    strcpy(result, base);
    #ifdef _WIN32
        if (base[base_len - 1] != '\\' && base[base_len - 1] != '/') {
            strcat(result, "\\");
        }
    #else
        if (base[base_len - 1] != '/') {
            strcat(result, "/");
        }
    #endif
    strcat(result, name);
    return result;
}

void ProcessSingleDirectory(const char* path, size_t dir_node_index) {
    FileTree* tree = cache_manager.building;
    if (!tree || dir_node_index >= tree->count) return;
    
#ifdef _WIN32
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", path);
    
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    
    if (find_handle == INVALID_HANDLE_VALUE) return;
    
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || 
            strcmp(find_data.cFileName, "..") == 0) continue;
        
        if (find_data.cFileName[0] == '.') continue;
        if (strcmp(find_data.cFileName, "node_modules") == 0) continue;
        if (strcmp(find_data.cFileName, ".git") == 0) continue;
        
        char* full_path = JoinPath(path, find_data.cFileName);
        size_t child_index;
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            child_index = AddNodeToTree(tree, TYPE_DIR, full_path, find_data.cFileName);
            if (child_index != SIZE_MAX) {
                AddPendingDir(full_path, child_index);
            }
        } else {
            child_index = AddNodeToTree(tree, TYPE_FILE, full_path, find_data.cFileName);
        }
        if (child_index != SIZE_MAX) {
            AddChildToTreeNode(tree, dir_node_index, child_index);
        }
        
        free(full_path);
        
    } while (FindNextFileA(find_handle, &find_data));
    
    FindClose(find_handle);
    
#else
    DIR* dir = opendir(path);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0) continue;
        
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "node_modules") == 0) continue;
        
        char* full_path = JoinPath(path, entry->d_name);
        struct stat entry_stat;
        
        if (stat(full_path, &entry_stat) == 0) {
            size_t child_index;
            
            if (S_ISDIR(entry_stat.st_mode)) {
                child_index = AddNodeToTree(tree, TYPE_DIR, full_path, entry->d_name);
                AddPendingDir(full_path, child_index);
            } else {
                child_index = AddNodeToTree(tree, TYPE_FILE, full_path, entry->d_name);
            }
            
            AddChildToTreeNode(tree, dir_node_index, child_index);
        }
        free(full_path);
    }
    closedir(dir);
#endif
}

void LogFileTreeNode(size_t node_index, int depth) {
    if (node_index >= cache_manager.active->count) return;
    
    Node* node = &cache_manager.active->nodes[node_index];
    
    char indent[256] = {0};
    for (int i = 0; i < depth && i < 127; i++) {
        strcat(indent, "  ");
    }
    
    const char* prefix = (node->type == TYPE_DIR) ? "[DIR] " : "[FILE]";
    
    TraceLog(LOG_INFO, "%s%s %s", indent, prefix, node->name);
    
    if (node->type == TYPE_DIR) {
        for (size_t i = 0; i < node->children_count; i++) {
            LogFileTreeNode(node->children[i], depth + 1);
        }
    }
}

void LogFileTree() {
    if (cache_manager.active == NULL || cache_manager.active->count == 0) {
        TraceLog(LOG_WARNING, "File cache is empty");
        return;
    }
    
    TraceLog(LOG_INFO, "========== FILE TREE ==========");
    //LogFileTreeNode(cache_manager.active->root_index, 0);
    //TraceLog(LOG_INFO, "===============================");
    TraceLog(LOG_INFO, "Total nodes: %zu", cache_manager.active->count);
}

void UpdateFileCache() {
    double current_time = GetTime();
    
    if (!cache_manager.rebuild_in_progress && 
        (current_time - cache_manager.last_scan_time > cache_manager.scan_interval || cache_manager.active == NULL)) {
        cache_manager.rebuild_requested = true;
    }
    
    if (cache_manager.rebuild_requested && !cache_manager.rebuild_in_progress) {
        cache_manager.rebuild_requested = false;
        cache_manager.rebuild_in_progress = true;
        
        if (cache_manager.building) {
            FreeFileTree(cache_manager.building);
        }
        cache_manager.building = CreateFileTree();

        if (!cache_manager.building) {
            cache_manager.rebuild_in_progress = false;
            return;
        }
    
        for (int i = 0; i < cache_manager.pending_count; i++) {
            if (cache_manager.pending_dirs[i].path) {
                free(cache_manager.pending_dirs[i].path);
            }
        }
        cache_manager.pending_count = 0;
        
        const char* name = strrchr(root_path, '/');
        #ifdef _WIN32
            const char* name_win = strrchr(root_path, '\\');
            if (name_win > name) name = name_win;
        #endif
        name = name ? name + 1 : root_path;
        
        cache_manager.building->root_index = AddNodeToTree(
            cache_manager.building, TYPE_DIR, root_path, name);
        if (cache_manager.building->root_index == SIZE_MAX) {
            FreeFileTree(cache_manager.building);
            cache_manager.building = NULL;
            cache_manager.rebuild_in_progress = false;
            return;
        }
        AddPendingDir(root_path, cache_manager.building->root_index);
    }
    
    if (cache_manager.rebuild_in_progress) {
        size_t dirs_to_process = cache_manager.dirs_per_frame;
        
        while (dirs_to_process > 0 && cache_manager.pending_count > 0) {
            PendingDir entry = cache_manager.pending_dirs[--cache_manager.pending_count];
            
            ProcessSingleDirectory(entry.path, entry.index);
            free(entry.path);
            dirs_to_process--;
        }
        
        if (cache_manager.pending_count == 0) {
            cache_manager.rebuild_in_progress = false;
            cache_manager.last_scan_time = current_time;
            
            FileTree* old = cache_manager.active;
            cache_manager.active = cache_manager.building;
            cache_manager.building = NULL;
            LogFileTree();
            FreeFileTree(old);
        }
    }
}

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

        char text[1025] = {0};
        size_t copyLen = (p.length < 1024) ? p.length : 1024;
        strncpy(text, buffer + p.start, copyLen);
        text[copyLen] = '\0';

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
    int buffer_start = 0;
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
    } else {       
        if (temp != NULL) {
            free(temp);
        } 
        temp = calloc(line_length + 1, sizeof(char));
        strncpy(temp, line, pointer.x);
        Vector2 draw_length = MeasureTextEx(editor_font, temp, fontSize, 1);
        RenderLineBuffer(temp, (Position){0, y_line}, pointer.x, (Vector2){xX, yY}, selection_start_position, selection_end_position);
        RenderLineBuffer(line + pointer.x, (Position){pointer.x, y_line}, line_length - pointer.x, (Vector2){xX + draw_length.x + pointerPaddingX * 2 + pointerWidth, yY}, selection_start_position, selection_end_position);   
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
    buf[len] = '\0';
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
            pointerPosition = PositionToPointer(found) + search_length;
            has_selection = true;
            selection_start = pointerPosition;
            selection_end = pointerPosition - search_length;
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

void RemoveSelection() {
    size_t selection_length = abs((int)(selection_end) - (int)(selection_start));
    RemoveArea(min(selection_start, selection_end), selection_length);
    has_selection = false;
}

void Paste() {
    char* buffer = (char*)GetClipboardText();
    if (buffer == NULL) {
        return;
    }

    if (has_selection) {
        RemoveSelection();
    }
    
    size_t buffer_length = strlen(buffer);
    char* normalized_buffer = (char*)malloc(buffer_length + 1);
    strcpy(normalized_buffer, buffer);

    normalize_line_endings(buffer);
    InsertStringAtPointer(buffer, buffer_length);
}

void Copy() {
    size_t selection_length = abs((int)selection_end - (int)selection_start);
    char* selection_buffer = GetTextRange(min(selection_start, selection_end), min(selection_start, selection_end) + selection_length);

    SetClipboardText(selection_buffer);

    free(selection_buffer);
}

void Cut() {
    Copy();
    RemoveSelection();
}

bool IsWordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsPunct(char c) {
    return c && !IsWordChar(c) && c != ' ' && c != '\t' && c != '\n';
}

void MoveWordRight() {
    size_t size = GetTextSize();
    if (pointerPosition >= size) return;
    char c = GetCharAt(pointerPosition);
    
    if (c == '\n') {
        pointerPosition++;
        return;
    }
    
    if (IsWordChar(c)) {
        while (pointerPosition < size && IsWordChar(GetCharAt(pointerPosition))) pointerPosition++;
    } else if (IsPunct(c)) {
        while (pointerPosition < size && IsPunct(GetCharAt(pointerPosition))) pointerPosition++;
    } else {
        while (pointerPosition < size && (c = GetCharAt(pointerPosition), c == ' ' || c == '\t')) pointerPosition++;
        if (pointerPosition < size && GetCharAt(pointerPosition) == '\n') return;
        while (pointerPosition < size && IsPunct(GetCharAt(pointerPosition))) pointerPosition++;
    }
}

void MoveWordLeft() {
    size_t size = GetTextSize();
    if (pointerPosition == 0) return;
    pointerPosition--;
    
    char c = GetCharAt(pointerPosition);
    if (c == '\n') return;
    
    while (pointerPosition > 0 && (c = GetCharAt(pointerPosition), c == ' ' || c == '\t')) {
        if (GetCharAt(pointerPosition - 1) == '\n') return;
        pointerPosition--;
    }
    
    c = GetCharAt(pointerPosition);
    if (IsWordChar(c)) {
        while (pointerPosition > 0 && IsWordChar(GetCharAt(pointerPosition - 1))) pointerPosition--;
    } else if (IsPunct(c)) {
        while (pointerPosition > 0 && IsPunct(GetCharAt(pointerPosition - 1))) pointerPosition--;
    }
}

void InsurePaddingModal(void* modal) {
    Modal* self = (Modal*)modal;
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    self->size.x = min(screen_width - self->padding.x, self->wanted_size.x);
    self->size.y = min(screen_height - self->padding.y, self->wanted_size.y);
}

void CenterModal(void* modal) {
    Modal* self = (Modal*)modal;
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    self->position.x = screen_width / 2.0f - self->size.x / 2.0f;
    self->position.y = screen_height / 2.0f - self->size.y / 2.0f;
}

void BeginRenderModal(Modal* modal) {
    for (int i = 0; i < modal->layout_count; i++) {
        modal->layouts[i](modal);
    }
    BeginScissorMode(modal->position.x, modal->position.y, modal->size.x, modal->size.y);
    DrawRectangle(modal->position.x, modal->position.y, modal->size.x, modal->size.y, modal->background_color);
}

void EndRenderModal(Modal* _modal) {
    EndScissorMode();
}
void RenderModal(Modal* modal) {
    BeginRenderModal(modal);
    if (modal->render) {
        modal->render(modal); 
    }
    EndRenderModal(modal);
}

void DebugModal(void* modal) {
    Modal* self = (Modal*)modal;
}

int main(int argc, char** argv) {
        size_t org_buffer_length = 0;

        if (argc >= 2) {
            struct stat path_stat;
            if (stat(argv[1], &path_stat) != 0) {
                fprintf(stderr, "Failed to access path: %s\n", argv[1]);
                org_buffer = strdup("");
                org_buffer_length = 0;
            } else if (S_ISDIR(path_stat.st_mode)) {
                fprintf(stderr, "Directory detected: %s\n", argv[1]);
                root_path = argv[1];
                UpdateFileCache();
                org_buffer = strdup("");
                org_buffer_length = 0;
            } else if (S_ISREG(path_stat.st_mode)) {
                org_buffer = load_file(argv[1], &org_buffer_length);
                if (!org_buffer) {
                    fprintf(stderr, "Failed to load file, using default text.\n");
                    org_buffer = strdup("");
                    org_buffer_length = 0;
                }
                normalize_line_endings(org_buffer);
            }
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
    MaximizeWindow();
    
    SetTargetFPS(60); 
    SetExitKey(KEY_NULL);
    
    editor_font = LoadFontEx("Input.ttf", fontSize, 0, 250);

    FileBrowserState browser_state = {0};
    Modal file_browser_modal = {
        .position = {0, 0},
        .size = {0, 0},
        .wanted_size = {800, 1000},
        .padding = {50, 50},
        .background_color = {40, 42, 48, 255},
        .layouts = (Layout[]){InsurePaddingModal, CenterModal},
        .layout_count = 2,
        .render = RenderFileBrowser,
        .input = InputFileBrowser,
        .state = &browser_state
    };

    LogFileTree();

    while (!WindowShouldClose() && !exit_requested) {
        if (root_path) {
            UpdateFileCache();
        }

        size_t screenWidth = GetScreenWidth();
        size_t screenHeight = GetScreenHeight();
        if (modal) {
            if (modal->input) {
                modal->input(modal);
            }
        } else if (!is_command_mode) {
            bool shift = IsKeyDown(KEY_LEFT_SHIFT);
            bool control = IsKeyDown(KEY_LEFT_CONTROL);
            int pointer_position_before = pointerPosition;
            if (IsKeyPressed(KEY_LEFT) && pointerPosition > 0) {
                if (control) {
                    MoveWordLeft();
                } else {
                    pointerPosition--;
                }
            }
            if (IsKeyPressed(KEY_RIGHT) && pointerPosition <= GetTextSize()) {
                if (control) {
                    MoveWordRight();
                } else {
                    pointerPosition++;
                }
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
            if (shift && !has_selection && pointer_position_before != pointerPosition) {
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
                    RemoveSelection();
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

                if (IsKeyPressed(KEY_V)) {
                    Paste();
                }

                if (IsKeyPressed(KEY_C)) {
                    Copy();
                }

                if (IsKeyPressed(KEY_X)) {
                    Cut();
                }

                if (IsKeyPressed(KEY_SPACE)) {
                    modal = &file_browser_modal;
                }
            } else {
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 126) {
                        if (has_selection) {
                            RemoveSelection();
                        }
                        InsertCharacterAtPointer(key);
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_TAB)) {
                    if (has_selection) {
                        RemoveSelection();
                    }
                    InsertStringAtPointer("  ", 2);
                }       
                if (IsKeyPressed(KEY_ENTER)) {    
                    if (has_selection) {
                        RemoveSelection();
                    }
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
        
        if (modal) {
            RenderModal(modal);
        }
        
        EndDrawing();
    }

    CloseWindow();
    free(org_buffer);
    return 0;
}
