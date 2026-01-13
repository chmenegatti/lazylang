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




static void lz_fn_main(void);

static void lz_fn_main(void)
{
    if (true) 
    {
        lz_runtime_log(lz_string_from_literal("hello"));
    }
}


int main(void) {
    lz_fn_main();
    return 0;
}
