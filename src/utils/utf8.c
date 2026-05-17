#include "utf8.h"

typedef struct {
    int mask;
    int comparator;
    size_t length;
    int first_byte_mask;
    size_t max_length;
} Utf8_length_entry;

static const Utf8_length_entry UTF8_LENGTH_LOOKUP_TABLE[] = {
    {0x80, 0x00, 1, 0x7f, 0x7F},
    {0xE0, 0xC0, 2, 0x1F, 0x7FF},
    {0xF0, 0xE0, 3, 0x0F, 0xFFFF},
    {0xF8, 0xF0, 4, 0x07, 0x10FFFF},
};

static const int UTF8_CONTINUATION_EXTRACTION_MASK = 0x3F;
static const int UTF8_CONTINUATION_MASK = 0xC0;
static const int UTF8_CONTINUATION_COMPARATOR = 0x80;

size_t utf8_decode(const char* p, uint32_t* codepoint) {
    for (size_t i = 0; i < 4; i++) {
        Utf8_length_entry entry = UTF8_LENGTH_LOOKUP_TABLE[i];
        if ((p[0] & entry.mask) == entry.comparator) {
            *codepoint = p[0] & entry.first_byte_mask;
            for (size_t j = 1; j < entry.length; j++) {
                *codepoint <<= 6;
                *codepoint |= (p[j] & UTF8_CONTINUATION_EXTRACTION_MASK);
            }
            return entry.length;
        }
    }

    return 0;
}

size_t utf8_encode(uint32_t codepoint, char* out) {
    for (size_t i = 0; i < 4; i++) {
        Utf8_length_entry entry = UTF8_LENGTH_LOOKUP_TABLE[i];
        if (codepoint <= entry.max_length) {
            for (size_t j = entry.length - 1; j >= 1; j--) {
                out[j] = UTF8_CONTINUATION_COMPARATOR | (codepoint & UTF8_CONTINUATION_EXTRACTION_MASK);
                codepoint >>= 6;
            }
            out[0] = entry.comparator | codepoint;
            return entry.length;
        }
    }
    return 0;
}

size_t utf8_prev(const char* start, const char* p) {
    size_t result = 0;
    do {
        p--;
        result++;
    } while (p > start && (*p & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_COMPARATOR);
    return result;
}

size_t utf8_strlen(const char* s) {
    const char* temp = s;
    size_t result = 0;
    
    uint32_t codepoint = 0;
    size_t length = 0;
    while (*temp != '\0') {
        length = utf8_decode(temp, &codepoint);
        result ++;
        temp += length;
    }

    return result;
}

size_t utf8_offset_to_codepoint(const char* s, size_t byte_offset) {
    size_t bytes_consumed = 0;
    size_t codepoints = 0;

    while (bytes_consumed < byte_offset && s[bytes_consumed] != '\0') {
        uint32_t cp;
        int len = utf8_decode(s + bytes_consumed, &cp);
        bytes_consumed += len;
        codepoints++;
    }

    return codepoints;
}

size_t utf8_codepoint_to_offset(const char* s, size_t code_offset) {
    size_t bytes_consumed = 0;
    size_t codepoints = 0;

    while (codepoints < code_offset && s[bytes_consumed] != '\0') {
        uint32_t cp;
        int len = utf8_decode(s + bytes_consumed, &cp);
        bytes_consumed += len;
        codepoints++;
    }

    return bytes_consumed;
}