/* Auto-generated C output from lazylang */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lz_string;
typedef struct lz_result {
    bool is_ok;
    union { void *ptr; int64_t i64; double f64; bool boolean; } value;
    union { void *ptr; int64_t i64; double f64; bool boolean; } error;
} lz_result;
typedef struct lz_maybe {
    bool has_value;
    union { void *ptr; int64_t i64; double f64; bool boolean; } data;
} lz_maybe;

#if defined(__GNUC__) || defined(__clang__)
#define LZ_UNUSED __attribute__((unused))
#else
#define LZ_UNUSED
#endif
static struct lz_string *lz_string_from_literal(const char *literal);
static void LZ_UNUSED lz_assign_int64(int64_t *dst, int64_t value);
static void LZ_UNUSED lz_assign_double(double *dst, double value);
static void LZ_UNUSED lz_assign_bool(bool *dst, bool value);
static void LZ_UNUSED lz_assign_string(struct lz_string **dst, struct lz_string *value);
static void LZ_UNUSED lz_assign_ptr(void **dst, void *value);
static void LZ_UNUSED lz_assign_result(lz_result *dst, lz_result value);
static void LZ_UNUSED lz_assign_maybe(lz_maybe *dst, lz_maybe value);
static void LZ_UNUSED lz_runtime_log(struct lz_string *value);
#undef log
#define log lz_runtime_log




static void lz_fn_main(void);

static void lz_fn_main(void)
{
    if (true) 
    {
        log(lz_string_from_literal("hello"));
    }
}


int main(void) {
    lz_fn_main();
    return 0;
}

struct lz_string {
    size_t length;
    const char *data;
};

static struct lz_string *lz_string_from_literal(const char *literal) {
    struct lz_string *str = malloc(sizeof(*str));
    if (!str) { fprintf(stderr, "Out of memory\n"); exit(1); }
    str->length = strlen(literal);
    str->data = literal;
    return str;
}

static void LZ_UNUSED lz_assign_int64(int64_t *dst, int64_t value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_double(double *dst, double value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_bool(bool *dst, bool value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_string(struct lz_string **dst, struct lz_string *value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_ptr(void **dst, void *value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_result(lz_result *dst, lz_result value) {
    *dst = value;
}
static void LZ_UNUSED lz_assign_maybe(lz_maybe *dst, lz_maybe value) {
    *dst = value;
}

static void lz_runtime_log(struct lz_string *value) {
    fwrite(value->data, 1, value->length, stdout);
    fputc('\n', stdout);
}
