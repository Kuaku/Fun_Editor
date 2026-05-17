#include "platform.h"

uint64_t GetTimeNs() {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int initialized = 0;

    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return (uint64_t)(counter.QuadPart * 1000000000 / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
#endif
}

#ifdef _WIN32
uint64_t PlatformGetMTime(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attr)) return 0;
    ULARGE_INTEGER uli;
    uli.LowPart = attr.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = attr.ftLastWriteTime.dwHighDateTime;

    return uli.QuadPart;
}

bool PlatformReadDirectory(const char* dir_path, FsEntryList* out) {
    out->count = 0;

    char search_path[PATH_MAX_LEN];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) {
            continue;
        }

        if (out->count == out->capacity) {
            out->capacity = out->capacity ? out->capacity * 2 : 64;
            out->items = (FsEntry*)realloc(out->items, out->capacity * sizeof(FsEntry));
        }

        FsEntry* e = &out->items[out->count++];
        strncpy(e->name, fd.cFileName, NAME_MAX_LEN - 1);
        e->name[NAME_MAX_LEN - 1] = '\0';
        e->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        ULARGE_INTEGER uli;
        uli.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        e->mtime = uli.QuadPart;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return true;
}

bool PlatformJoinPath(char* out, size_t out_size, const char* a, const char* b) {
    int written = snprintf(out, out_size, "%s\\%s", a, b);
    if (written < 0 || written >= out_size) {
        fprintf(stderr, "Warning: Path too long, skipping: %s\\%s\n", a, b);
        return false;
    }
    return true;
}

bool PlatformMakeDir(const char* abs_path) {
    return CreateDirectoryA(abs_path, NULL) != 0
        || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool PlatformMakeFile(const char* abs_path) {
    HANDLE h = CreateFileA(
        abs_path,
        GENERIC_WRITE,
        0, NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}
#else
uint64_t PlatformGetMTime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (uint64_t)st.st_mtime;
}

bool PlatformReadDirectory(const char* dir_path, FsEntryList* out) {
    out->count = 0;

    DIR* dir = opendir(dir_path);
    if (!dir) return false;

    struct dirent* de;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
            continue;
        }

        if (out->count == out->capacity) {
            out->capacity = out->capacity ? out->capacity * 2 : 64;
            out->items = (FsEntry*)realloc(out->items, out->capacity * sizeof(FsEntry));
        }

        FsEntry* e = &out->items[out->count];
        strncpy(e->name, de->d_name, NAME_MAX_LEN - 1);
        e->name[NAME_MAX_LEN - 1] = '\0';

        char full[PATH_MAX_LEN];
        snprintf(full, sizeof(full), "%s/%s", dir_path, de->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        e->is_dir = S_ISDIR(st.st_mode);
        e->mtime  = (uint64_t)st.st_mtime;
        out->count++;
    }

    closedir(dir);
    return true;
}

bool PlatformJoinPath(char* out, size_t out_size, const char* a, const char* b) {
    int written = snprintf(out, out_size, "%s/%s", a, b);
    if (written < 0 || written >= out_size) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", a, b);
        return false;
    }
    return true;
}

bool PlatformMakeDir(const char* abs_path) {
    return mkdir(abs_path, 0755) == 0
        || errno == EEXIST;
}

bool PlatformMakeFile(const char* abs_path) {
    int fd = open(abs_path, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) return false;
    close(fd);
    return true;
}
#endif

bool CreatePathUnderRoot(const char* root_path, const char* input) {
    if (!input || input[0] == '\0') return false;

    size_t input_len = strlen(input);
    bool trailing_slash = (input[input_len - 1] == '/' || input[input_len - 1] == '\\');

    char input_copy[PATH_MAX_LEN];
    strncpy(input_copy, input, PATH_MAX_LEN - 1);
    input_copy[PATH_MAX_LEN - 1] = '\0';

    size_t copy_len = strlen(input_copy);
    if (copy_len > 0 && (input_copy[copy_len - 1] == '/' || input_copy[copy_len - 1] == '\\')) {
        input_copy[copy_len - 1] = '\0';
    }

    char* segments[256];
    int segment_count = 0;

    char* seg = strtok(input_copy, "/\\");
    while (seg && segment_count < 256) {
        segments[segment_count++] = seg;
        seg = strtok(NULL, "/\\");
    }

    if (segment_count == 0) return false;

    char current[PATH_MAX_LEN];
    char next[PATH_MAX_LEN];
    strncpy(current, root_path, PATH_MAX_LEN - 1);
    current[PATH_MAX_LEN - 1] = '\0';

    int dir_count = trailing_slash ? segment_count : segment_count - 1;

    for (int i = 0; i < dir_count; i++) {
        if (!PlatformJoinPath(next, sizeof(next), current, segments[i])) {
            fprintf(stderr, "CreatePathUnderRoot: path too long at segment '%s'\n", segments[i]);
            return false;
        }

        strncpy(current, next, PATH_MAX_LEN - 1);
        current[PATH_MAX_LEN - 1] = '\0';
        if (!PlatformMakeDir(current)) {
            fprintf(stderr, "CreatePathUnderRoot: failed to create dir '%s'\n", current);
            return false;
        }
    }

    if (!trailing_slash) {
        if (!PlatformJoinPath(next, sizeof(next), current, segments[segment_count - 1])) {
            fprintf(stderr, "CreatePathUnderRoot: path too long for file '%s'\n", segments[segment_count - 1]);
            return false;
        }
        if (!PlatformMakeFile(next)) {
            fprintf(stderr, "CreatePathUnderRoot: failed to create file '%s'\n", next);
            return false;
        }
    }

    return true;
}

void StrToLower(const char* src, char* dst, size_t dst_size) {
    size_t i = 0;
    for (; src[i] && i < dst_size - 1; i++) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

FileType GetFileTypeFromPath(const char* path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return TYPE_ERROR;
    } else if (S_ISDIR(path_stat.st_mode)) {
        return TYPE_DIR;
    } else if (S_ISREG(path_stat.st_mode)) {
        return TYPE_FILE;
    }

    return TYPE_ERROR;
}

char* LoadFile(const char* filename, size_t* out_len) {
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
