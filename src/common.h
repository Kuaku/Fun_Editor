#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

#include "raylib.h"

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
    #include <limits.h>
    #include <time.h>
    #include <errno.h>
    #include <fcntl.h>
#endif

#ifndef _WIN32
    #define min(a, b) ((a) < (b) ? (a) : (b))
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef _WIN32
    #define PATH_MAX_LEN MAX_PATH
#else
    #define PATH_MAX_LEN PATH_MAX
#endif
#define NAME_MAX_LEN 256

#define BREAK_DOWN_RECT(rect) rect.position.x, rect.position.y, rect.size.x, rect.size.y
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define INITIAL_TEXT_BUFFER_CAPACITY 10
#define INITIAL_ADD_BUFFER_CAPACITY 4096
#define INITIAL_UNDO_STACK_CAPACITY 4096
#define INITIAL_PIECE_BUFFER_CAPACITY 1024
#define INITIAL_COMMAND_BUFFER_CAPACITY 1024
#define INITIAL_COMMAND_BINDING_CAPACITY 1024
#define INITIAL_TOKENIZER_BUFFER_CAPACITY 8
#define INITIAL_MODAL_BUFFER_CAPACITY 8
#define INITIAL_MODAL_CACHE_BUFFER_CAPACITY 8
#define INITIAL_FILE_TREE_CAPACITY 256
#define INITIAL_RENDER_QUEUE_CAPACITY 256
#define INITIAL_STRING_ARENA_CAPACITY 1024
#define INITIAL_RENDER_TREE_CAPACITY 16

#define INITIAL_SCAN_INTERVAL 5.0
#define INITIAL_DIRS_PER_FRAME 50

typedef struct Editor Editor;
typedef struct FileSystem FileSystem;

typedef enum { ORIGINAL, ADD } BufferType;
typedef enum { TYPE_DIR, TYPE_FILE, TYPE_ERROR } FileType;
typedef enum { MODE_TEXT, MODE_COMMAND, MODE_COUNT } EditorMode;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Position position;
    Position size;
} Rect;

typedef struct {
    uint32_t index;
    uint32_t generation;
} CacheHandle;

#define CACHE_HANDLE_INVALID ((CacheHandle){ UINT32_MAX, 0 })

static inline bool cache_handle_eq(CacheHandle a, CacheHandle b) {
    return a.index == b.index && a.generation == b.generation;
}

static inline bool cache_handle_is_invalid(CacheHandle h) {
    return h.index == UINT32_MAX;
}

Vector2 PositionToVector(Position position);

#endif
