#ifndef LZ_RUNTIME_H
#define LZ_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Runtime invariants
 * ------------------
 * - All lazylang programs that reach this layer already passed semantic
 *   analysis (SEMA) and code generation; the runtime never re-validates the AST.
 * - Codegen is assumed correct. If something is wrong, the fix lives in SEMA or
 *   the backend, never in the runtime.
 * - The runtime does not attempt to perform type checking, semantic
 *   enforcement, or any higher-level language logic. It simply exposes the
 *   helpers that the current C backend requires.
 * - This module exists solely to support the current backend surface. Any
 *   future backend changes must continue to honor this contract or extend it
 *   explicitly; otherwise, fixes belong in SEMA/codegen, not here.
 */

/*
 * Defining LZ_RUNTIME_DEFINE_STRUCTS before including this header exposes the
 * concrete layout of the runtime types so that generated C code can stack
 * allocate them. Without the define, the structs remain opaque, which allows
 * future runtime revisions (e.g., ARC) without leaking implementation details.
 */
typedef struct lz_string lz_string;
typedef struct lz_result lz_result;
typedef struct lz_maybe lz_maybe;

/*
 * lz_string ownership model
 * -------------------------
 * - The runtime always owns the lz_string struct itself, regardless of where it
 *   was created.
 * - When constructed via lz_string_from_literal, the underlying character data
 *   remains owned by the static literal; lz_string only references it and must
 *   never attempt to free it.
 * - Future heap-allocated strings will make lz_string responsible for both the
 *   struct and the backing buffer, paving the way for ARC/reference counting.
 * - lz_string_release is the single extension point for that future work, so it
 *   must be called (even today) whenever ownership transfer semantics require it.
 */

#ifdef LZ_RUNTIME_DEFINE_STRUCTS
struct lz_string {
    size_t length;
    const char *data;
};

struct lz_result {
    bool is_ok;
    union { void *ptr; int64_t i64; double f64; bool boolean; } value;
    union { void *ptr; int64_t i64; double f64; bool boolean; } error;
};

struct lz_maybe {
    bool has_value;
    union { void *ptr; int64_t i64; double f64; bool boolean; } data;
};
#endif

lz_string *lz_string_from_literal(const char *literal);
const char *lz_string_data(const lz_string *value);
size_t lz_string_length(const lz_string *value);
void lz_string_release(lz_string *value);

/*
 * Assignment hooks (lz_assign_string/lz_assign_ptr/lz_assign_result/lz_assign_maybe)
 * centralize every observable mutation so that future ARC/reference counting can
 * intercept writes. They must never be bypassed, inlined, or removed, even
 * though they currently perform simple assignments.
 */
void lz_assign_int64(int64_t *dst, int64_t value);
void lz_assign_double(double *dst, double value);
void lz_assign_bool(bool *dst, bool value);
/* Handles string ownership handoff; never bypass. */
void lz_assign_string(lz_string **dst, lz_string *value);
/* Pointer assignment funnel for future ARC hooks. */
void lz_assign_ptr(void **dst, void *value);
/* Result assignment funnel for future ARC hooks. */
void lz_assign_result(lz_result *dst, lz_result value);
/* Maybe assignment funnel for future ARC hooks. */
void lz_assign_maybe(lz_maybe *dst, lz_maybe value);

void lz_runtime_log(lz_string *value);

#endif
