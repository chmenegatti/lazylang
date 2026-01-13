#define LZ_RUNTIME_DEFINE_STRUCTS
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *lz_runtime_alloc(size_t size) {
    void *ptr = calloc(1, size);
    if (!ptr) {
        fprintf(stderr, "lazylang runtime: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

lz_string *lz_string_from_literal(const char *literal) {
    if (!literal) {
        return NULL;
    }
    lz_string *str = lz_runtime_alloc(sizeof(*str));
    str->length = strlen(literal);
    str->data = literal;
    return str;
}

const char *lz_string_data(const lz_string *value) {
    return value ? value->data : NULL;
}

size_t lz_string_length(const lz_string *value) {
    return value ? value->length : 0;
}

void lz_string_release(lz_string *value) {
    (void)value;
    /* ARC hook: no-op for now */
}

void lz_assign_int64(int64_t *dst, int64_t value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_double(double *dst, double value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_bool(bool *dst, bool value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_string(lz_string **dst, lz_string *value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_ptr(void **dst, void *value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_result(lz_result *dst, lz_result value) {
    if (dst) {
        *dst = value;
    }
}

void lz_assign_maybe(lz_maybe *dst, lz_maybe value) {
    if (dst) {
        *dst = value;
    }
}

void lz_runtime_log(lz_string *value) {
    if (!value) {
        return;
    }
    fwrite(value->data, 1, value->length, stdout);
    fputc('\n', stdout);
}
