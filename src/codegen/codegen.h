#ifndef LZ_CODEGEN_H
#define LZ_CODEGEN_H

#include "../ast/ast.h"

#include <stdbool.h>

typedef struct {
    const char *c_output_path;
    const char *binary_output_path;
    bool emit_binary;
} CodegenOptions;

bool codegen_emit(const ASTProgram *program, const CodegenOptions *options);

#endif
