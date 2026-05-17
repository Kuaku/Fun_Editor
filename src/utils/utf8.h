#ifndef UTF8_H
#define UTF8_H

#include "../common.h"

size_t utf8_decode(const char* p, uint32_t* codepoint);
size_t utf8_encode(uint32_t codepoint, char* out); 
size_t utf8_prev(const char* start, const char* p);
size_t utf8_strlen(const char* s);
size_t utf8_offset_to_codepoint(const char* s, size_t byte_offset);
size_t utf8_codepoint_to_offset(const char* s, size_t code_offset);

#endif