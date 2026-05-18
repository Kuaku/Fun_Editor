#ifndef PLATFORM_H
#define PLATFORM_H

#include "../common.h"

static char FILE_SYSTEM_SEPERATER = 
                #ifdef _WIN32
                        '\\'
                #else
                        '/'
                #endif
                ;

typedef struct {
    char     name[NAME_MAX_LEN];
    bool     is_dir;
    uint64_t mtime;
} FsEntry;

typedef struct {
    FsEntry* items;
    size_t   count;
    size_t   capacity;
} FsEntryList;

uint64_t GetTimeNs(void);

uint64_t PlatformGetMTime(const char* path);

bool PlatformReadDirectory(const char* dir_path, FsEntryList* out);

bool PlatformJoinPath(char* out, size_t out_size, const char* a, const char* b);

bool PlatformMakeDir(const char* abs_path);

bool PlatformMakeFile(const char* abs_path);

bool CreatePathUnderRoot(const char* root_path, const char* input);

void StrToLower(const char* src, char* dst, size_t dst_size);

FileType GetFileTypeFromPath(const char* path);

char* LoadFile(const char* filename, size_t* out_len);

void normalize_line_endings(char* buf);

#endif
