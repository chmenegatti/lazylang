/* Auto-generated C output from lazylang */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__GNUC__) || defined(__clang__)
#define LZ_UNUSED __attribute__((unused))
#else
#define LZ_UNUSED
#endif
#define LZ_RUNTIME_DEFINE_STRUCTS
#include "src/runtime/runtime.h"




static bool lz_fn_is_positive(int64_t x);
static void lz_fn_main(void);

static bool lz_fn_is_positive(int64_t x)
{
    bool __lz_ret = {0};
    if ((x > 0)) 
    {
        lz_assign_bool(&__lz_ret, true);
    }
    else
    {
        lz_assign_bool(&__lz_ret, false);
    }
    return __lz_ret;
}

static void lz_fn_main(void)
{
    if (lz_fn_is_positive(10)) 
    {
        lz_runtime_log(lz_string_from_literal("positive"));
    }
    else
    {
        lz_runtime_log(lz_string_from_literal("negative"));
    }
}


int main(void) {
    lz_fn_main();
    return 0;
}
