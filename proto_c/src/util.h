#pragma once

/////////////////
// UTILITY MACROS
#define STATEMENT(X) do { (X); } while(0)
#define UNREF(X) STATEMENT((void)(X))
#define TODO(TODO_MSG) assert(false && "Todo! " TODO_MSG)

//////////////////
// BUFFER HELPERS
#define ARRAYSIZE(x) (sizeof((x))/sizeof((x)[0]))

//////////////////
// STRING HELPERS
typedef struct str_t {
    char* bytes;
    uint64_t len;
} str_t;

#define cstr(s) { .bytes=(s), .len=ARRAYSIZE(s)-1 }
inline bool cstr_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }
inline bool mem_eq(const void* a, const void* b, size_t len) { return memcmp(a, b, len) == 0; }
inline bool str_eq(str_t a, str_t b) { return a.len == b.len && mem_eq(a.bytes, b.bytes, a.len); }

bool
str_starts_with(
    str_t s,
    str_t prefix);

str_t
make_str(
    const char* s);

//////////////////
// LOGGING HELPERS
#define Log(FMT, ...) printf(FMT "\n", __VA_ARGS__)

///////////////
// SPAN HELPERS
#define DEF_SPAN_T(type, typename) \
    typedef struct typename { \
        type* data; \
        size_t count; \
    } typename

#define SPAN_ADV(span) \
    do { \
        (span).data += 1; \
        (span).count -= 1; \
    } while (0) \

#define SPAN_FIRST(span, cnt) \
    { \
        .data = (span).data, \
        .count = min((span).count, cnt), \
    }

DEF_SPAN_T(const char*, char_ptr_span_t);
DEF_SPAN_T(uint8_t, byte_span_t);

byte_span_t
get_first_split(
    byte_span_t buffer,
    uint8_t split_char);

byte_span_t
get_next_split(
    const byte_span_t current_split,
    const byte_span_t full_span,
    uint8_t split_char);

#define split_by(span_t, iter_var_name, buffer, split_char) \
    for ( \
        span_t iter_var_name = get_first_split((buffer), (split_char)); \
        iter_var_name.data != NULL; \
        iter_var_name = get_next_split((iter_var_name), (buffer), (split_char)) \
        ) \
