#include "codegen.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_C_OUTPUT "lazylang_out.c"
#define DEFAULT_BINARY_OUTPUT "lazylang_out"
#define INDENT_WIDTH 4

typedef struct {
    FILE *file;
    int indent;
} CodeWriter;

typedef struct {
    const ASTStructDecl *decl;
    char *name;
    char *assign_helper;
} CGStructInfo;

typedef struct {
    const ASTFunctionDecl *decl;
    char *name;
    char *c_name;
} CGFunctionInfo;

typedef struct {
    const char *name;
    const char *type_name;
    bool is_mutable;
} CGVarBinding;

typedef struct {
    CGVarBinding *items;
    size_t count;
    size_t capacity;
} CGScope;

typedef struct {
    CodeWriter writer;
    const ASTProgram *program;
    CGStructInfo *structs;
    size_t struct_count;
    size_t struct_capacity;
    CGFunctionInfo *functions;
    size_t function_count;
    size_t function_capacity;
    CGScope *scopes;
    size_t scope_count;
    size_t scope_capacity;
    const ASTFunctionDecl *current_function;
    bool had_error;
} CodegenContext;

static void writer_write_indent(CodeWriter *writer);
static void writer_line(CodeWriter *writer, const char *fmt, ...);
static void writer_printf(CodeWriter *writer, const char *fmt, ...);
static void writer_begin_line(CodeWriter *writer);
static void writer_end_line(CodeWriter *writer);
static void writer_push(CodeWriter *writer);
static void writer_pop(CodeWriter *writer);
static void writer_blank_line(CodeWriter *writer);

static char *cg_strdup(const char *text);
static void cg_context_init(CodegenContext *ctx,
                            FILE *out,
                            const ASTProgram *program);
static void cg_context_destroy(CodegenContext *ctx);
static void cg_collect_metadata(CodegenContext *ctx);
static void cg_register_struct(CodegenContext *ctx, const ASTStructDecl *decl);
static void cg_register_function(CodegenContext *ctx, const ASTFunctionDecl *decl);
static const CGFunctionInfo *cg_find_function(const CodegenContext *ctx, const char *name);
static const CGStructInfo *cg_find_struct(const CodegenContext *ctx, const char *name);
static void cg_scope_push(CodegenContext *ctx);
static void cg_scope_pop(CodegenContext *ctx);
static void cg_scope_add(CodegenContext *ctx,
                         const char *name,
                         const char *type_name,
                         bool is_mutable);
static const CGVarBinding *cg_scope_lookup(const CodegenContext *ctx, const char *name);
static bool cg_emit_program(CodegenContext *ctx);
static void cg_emit_file_header(CodegenContext *ctx);
static void cg_emit_includes(CodegenContext *ctx);
static void cg_emit_struct_forward_decls(CodegenContext *ctx);
static void cg_emit_structs(CodegenContext *ctx);
static void cg_emit_struct_assign_helpers(CodegenContext *ctx);
static void cg_emit_function_signature(CodegenContext *ctx,
                                       const CGFunctionInfo *info,
                                       bool prototype);
static void cg_emit_function_prototypes(CodegenContext *ctx);
static void cg_emit_function_body(CodegenContext *ctx, const ASTFunctionDecl *fn);
static void cg_emit_function_definitions(CodegenContext *ctx);
static void cg_emit_entrypoint(CodegenContext *ctx);
static void cg_emit_block(CodegenContext *ctx,
                          ASTBlock *block,
                          const char *tail_var,
                          const char *tail_helper);
static void cg_emit_statement(CodegenContext *ctx,
                              ASTNode *node,
                              const char *tail_var,
                              const char *tail_helper);
static void cg_emit_var_decl(CodegenContext *ctx, ASTVarDecl *decl);
static void cg_emit_assignment(CodegenContext *ctx, ASTAssignStmt *assign);
static void cg_emit_if(CodegenContext *ctx,
                       ASTIfStmt *stmt,
                       const char *tail_var,
                       const char *tail_helper);
static void cg_emit_return(CodegenContext *ctx, ASTReturnStmt *stmt);
static void cg_emit_expr_stmt(CodegenContext *ctx,
                              ASTExprStmt *stmt,
                              const char *tail_var,
                              const char *tail_helper);
static void cg_emit_expression(CodegenContext *ctx, ASTNode *node);
static void cg_emit_literal(CodegenContext *ctx, ASTLiteralExpr *literal);
static void cg_emit_identifier(CodegenContext *ctx, ASTIdentifierExpr *ident);
static void cg_emit_call(CodegenContext *ctx, ASTCallExpr *call);
static void cg_emit_binary(CodegenContext *ctx, ASTBinaryExpr *binary);
static const char *cg_binary_op(TokenType type);
static void cg_emit_string_literal(CodegenContext *ctx, const char *text);
static const char *cg_c_type_for(const CodegenContext *ctx, const char *type_name);
static const char *cg_c_return_type_for(const CodegenContext *ctx, const char *type_name);
static const char *cg_assign_helper_for(const CodegenContext *ctx, const char *type_name);
static bool cg_type_is_result(const char *type_name);
static bool cg_type_is_maybe(const char *type_name);
static bool cg_type_is_struct(const CodegenContext *ctx, const char *type_name);
static bool cg_fail(CodegenContext *ctx, const Token *token, const char *message);
static void cg_emit_assignment_call(CodegenContext *ctx,
                                    const char *target_name,
                                    const char *type_name,
                                    ASTNode *value);
static bool cg_command_exists(const char *cmd);
static bool cg_invoke_compiler(const char *compiler,
                               const char *c_path,
                               const char *binary_path);
static bool cg_run_clang(const char *c_path, const char *binary_path);

bool codegen_emit(const ASTProgram *program, const CodegenOptions *options) {
    if (!program) {
        return false;
    }

    const char *c_path = (options && options->c_output_path)
        ? options->c_output_path
        : DEFAULT_C_OUTPUT;
    const char *binary_path = (options && options->binary_output_path)
        ? options->binary_output_path
        : DEFAULT_BINARY_OUTPUT;
    bool emit_binary = true;
    if (options) {
        emit_binary = options->emit_binary;
    }

    FILE *out = fopen(c_path, "w");
    if (!out) {
        fprintf(stderr, "failed to open '%s' for writing: %s\n", c_path, strerror(errno));
        return false;
    }

    CodegenContext ctx;
    cg_context_init(&ctx, out, program);
    bool ok = cg_emit_program(&ctx);
    cg_context_destroy(&ctx);
    fclose(out);

    if (ok && emit_binary) {
        ok = cg_run_clang(c_path, binary_path);
    }

    return ok;
}

static void writer_write_indent(CodeWriter *writer) {
    for (int i = 0; i < writer->indent; i++) {
        fputs("    ", writer->file);
    }
}

static void writer_line(CodeWriter *writer, const char *fmt, ...) {
    writer_write_indent(writer);
    va_list args;
    va_start(args, fmt);
    vfprintf(writer->file, fmt, args);
    va_end(args);
    fputc('\n', writer->file);
}

static void writer_printf(CodeWriter *writer, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(writer->file, fmt, args);
    va_end(args);
}

static void writer_begin_line(CodeWriter *writer) {
    writer_write_indent(writer);
}

static void writer_end_line(CodeWriter *writer) {
    fputc('\n', writer->file);
}

static void writer_push(CodeWriter *writer) {
    writer->indent++;
}

static void writer_pop(CodeWriter *writer) {
    if (writer->indent > 0) {
        writer->indent--;
    }
}

static void writer_blank_line(CodeWriter *writer) {
    fputc('\n', writer->file);
}

static char *cg_strdup(const char *text) {
    if (!text) {
        return NULL;
    }
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void cg_context_init(CodegenContext *ctx,
                            FILE *out,
                            const ASTProgram *program) {
    ctx->writer.file = out;
    ctx->writer.indent = 0;
    ctx->program = program;
    ctx->structs = NULL;
    ctx->struct_count = 0;
    ctx->struct_capacity = 0;
    ctx->functions = NULL;
    ctx->function_count = 0;
    ctx->function_capacity = 0;
    ctx->scopes = NULL;
    ctx->scope_count = 0;
    ctx->scope_capacity = 0;
    ctx->current_function = NULL;
    ctx->had_error = false;
}

static void cg_context_destroy(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->struct_count; i++) {
        free(ctx->structs[i].name);
        free(ctx->structs[i].assign_helper);
    }
    free(ctx->structs);

    for (size_t i = 0; i < ctx->function_count; i++) {
        free(ctx->functions[i].name);
        free(ctx->functions[i].c_name);
    }
    free(ctx->functions);

    for (size_t i = 0; i < ctx->scope_count; i++) {
        free(ctx->scopes[i].items);
    }
    free(ctx->scopes);
}

static void cg_collect_metadata(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->program->declarations.count; i++) {
        ASTNode *node = ctx->program->declarations.items[i];
        if (node->kind == AST_NODE_STRUCT) {
            cg_register_struct(ctx, (const ASTStructDecl *)node);
        } else if (node->kind == AST_NODE_FUNCTION) {
            cg_register_function(ctx, (const ASTFunctionDecl *)node);
        }
    }
}

static void cg_register_struct(CodegenContext *ctx, const ASTStructDecl *decl) {
    if (ctx->struct_count == ctx->struct_capacity) {
        size_t new_capacity = ctx->struct_capacity ? ctx->struct_capacity * 2 : 4;
        CGStructInfo *new_items = realloc(ctx->structs, new_capacity * sizeof(CGStructInfo));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        ctx->structs = new_items;
        ctx->struct_capacity = new_capacity;
    }
    CGStructInfo *info = &ctx->structs[ctx->struct_count++];
    info->decl = decl;
    info->name = cg_strdup(decl->name);
    size_t helper_len = strlen(decl->name) + strlen("lz_assign_struct_") + 1;
    info->assign_helper = malloc(helper_len);
    if (!info->assign_helper) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    snprintf(info->assign_helper, helper_len, "lz_assign_struct_%s", decl->name);
}

static void cg_register_function(CodegenContext *ctx, const ASTFunctionDecl *decl) {
    if (ctx->function_count == ctx->function_capacity) {
        size_t new_capacity = ctx->function_capacity ? ctx->function_capacity * 2 : 4;
        CGFunctionInfo *new_items = realloc(ctx->functions, new_capacity * sizeof(CGFunctionInfo));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        ctx->functions = new_items;
        ctx->function_capacity = new_capacity;
    }
    CGFunctionInfo *info = &ctx->functions[ctx->function_count++];
    info->decl = decl;
    info->name = cg_strdup(decl->name);
    size_t prefix_len = strlen("lz_fn_");
    size_t c_name_len = prefix_len + strlen(decl->name) + 1;
    info->c_name = malloc(c_name_len);
    if (!info->c_name) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    snprintf(info->c_name, c_name_len, "lz_fn_%s", decl->name);
}

static const CGFunctionInfo *cg_find_function(const CodegenContext *ctx, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < ctx->function_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0) {
            return &ctx->functions[i];
        }
    }
    return NULL;
}

static const CGStructInfo *cg_find_struct(const CodegenContext *ctx, const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < ctx->struct_count; i++) {
        if (strcmp(ctx->structs[i].name, name) == 0) {
            return &ctx->structs[i];
        }
    }
    return NULL;
}

static void cg_scope_push(CodegenContext *ctx) {
    if (ctx->scope_count == ctx->scope_capacity) {
        size_t new_capacity = ctx->scope_capacity ? ctx->scope_capacity * 2 : 4;
        CGScope *new_scopes = realloc(ctx->scopes, new_capacity * sizeof(CGScope));
        if (!new_scopes) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        ctx->scopes = new_scopes;
        ctx->scope_capacity = new_capacity;
    }
    CGScope *scope = &ctx->scopes[ctx->scope_count++];
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static void cg_scope_pop(CodegenContext *ctx) {
    if (ctx->scope_count == 0) {
        return;
    }
    CGScope *scope = &ctx->scopes[ctx->scope_count - 1];
    free(scope->items);
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
    ctx->scope_count--;
}

static void cg_scope_add(CodegenContext *ctx,
                         const char *name,
                         const char *type_name,
                         bool is_mutable) {
    if (ctx->scope_count == 0) {
        cg_scope_push(ctx);
    }
    CGScope *scope = &ctx->scopes[ctx->scope_count - 1];
    if (scope->count == scope->capacity) {
        size_t new_capacity = scope->capacity ? scope->capacity * 2 : 4;
        CGVarBinding *new_items = realloc(scope->items, new_capacity * sizeof(CGVarBinding));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        scope->items = new_items;
        scope->capacity = new_capacity;
    }
    scope->items[scope->count++] = (CGVarBinding){
        .name = name,
        .type_name = type_name,
        .is_mutable = is_mutable,
    };
}

static const CGVarBinding *cg_scope_lookup(const CodegenContext *ctx, const char *name) {
    if (!name) {
        return NULL;
    }
    for (size_t s = ctx->scope_count; s > 0; s--) {
        CGScope *scope = &ctx->scopes[s - 1];
        for (size_t i = 0; i < scope->count; i++) {
            if (strcmp(scope->items[i].name, name) == 0) {
                return &scope->items[i];
            }
        }
    }
    return NULL;
}

static bool cg_emit_program(CodegenContext *ctx) {
    cg_collect_metadata(ctx);
    cg_emit_file_header(ctx);
    cg_emit_includes(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_struct_forward_decls(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_structs(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_struct_assign_helpers(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_function_prototypes(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_function_definitions(ctx);
    writer_blank_line(&ctx->writer);
    cg_emit_entrypoint(ctx);
    return !ctx->had_error;
}

static void cg_emit_file_header(CodegenContext *ctx) {
    writer_line(&ctx->writer, "/* Auto-generated C output from lazylang */");
}

static void cg_emit_includes(CodegenContext *ctx) {
    writer_line(&ctx->writer, "#include <stdint.h>");
    writer_line(&ctx->writer, "#include <stdbool.h>");
    writer_line(&ctx->writer, "#include <stddef.h>");
    writer_line(&ctx->writer, "#include <stdio.h>");
    writer_line(&ctx->writer, "#include <stdlib.h>");
    writer_line(&ctx->writer, "#include <string.h>");
    writer_line(&ctx->writer, "#if defined(__GNUC__) || defined(__clang__)");
    writer_line(&ctx->writer, "#define LZ_UNUSED __attribute__((unused))");
    writer_line(&ctx->writer, "#else");
    writer_line(&ctx->writer, "#define LZ_UNUSED");
    writer_line(&ctx->writer, "#endif");
    writer_line(&ctx->writer, "#define LZ_RUNTIME_DEFINE_STRUCTS");
    writer_line(&ctx->writer, "#include \"src/runtime/runtime.h\"");
}


static void cg_emit_struct_forward_decls(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->struct_count; i++) {
        writer_line(&ctx->writer, "typedef struct %s %s;", ctx->structs[i].name, ctx->structs[i].name);
    }
}

static void cg_emit_structs(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->struct_count; i++) {
        const ASTStructDecl *decl = ctx->structs[i].decl;
        writer_line(&ctx->writer, "struct %s {", decl->name);
        writer_push(&ctx->writer);
        for (size_t f = 0; f < decl->fields.count; f++) {
            ASTStructField *field = decl->fields.items[f];
            const char *c_type = cg_c_type_for(ctx, field->type_name);
            writer_line(&ctx->writer, "%s %s;", c_type, field->name);
        }
        writer_pop(&ctx->writer);
        writer_line(&ctx->writer, "};");
        writer_blank_line(&ctx->writer);
    }
}

static void cg_emit_struct_assign_helpers(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->struct_count; i++) {
        const CGStructInfo *info = &ctx->structs[i];
        writer_line(&ctx->writer,
                    "static void LZ_UNUSED %s(%s *dst, %s value) {",
                    info->assign_helper,
                    info->name,
                    info->name);
        writer_push(&ctx->writer);
        writer_line(&ctx->writer, "*dst = value;");
        writer_pop(&ctx->writer);
        writer_line(&ctx->writer, "}");
        writer_blank_line(&ctx->writer);
    }
}

static void cg_emit_function_signature(CodegenContext *ctx,
                                       const CGFunctionInfo *info,
                                       bool prototype) {
    const ASTFunctionDecl *fn = info->decl;
    const char *ret_type = cg_c_return_type_for(ctx, fn->return_type);
    writer_begin_line(&ctx->writer);
    writer_printf(&ctx->writer, "static %s %s(", ret_type, info->c_name);
    if (fn->params.count == 0) {
        writer_printf(&ctx->writer, "void");
    } else {
        for (size_t i = 0; i < fn->params.count; i++) {
            ASTFunctionParam *param = fn->params.items[i];
            const char *param_type = cg_c_type_for(ctx, param->type_name);
            writer_printf(&ctx->writer,
                          "%s %s",
                          param_type,
                          param->name);
            if (i + 1 < fn->params.count) {
                writer_printf(&ctx->writer, ", ");
            }
        }
    }
    writer_printf(&ctx->writer, ")");
    if (prototype) {
        writer_printf(&ctx->writer, ";");
    }
    writer_end_line(&ctx->writer);
}

static void cg_emit_function_prototypes(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->function_count; i++) {
        cg_emit_function_signature(ctx, &ctx->functions[i], true);
    }
}

static void cg_emit_function_body(CodegenContext *ctx, const ASTFunctionDecl *fn) {
    if (!fn->body) {
        writer_line(&ctx->writer, "{");
        writer_line(&ctx->writer, "}");
        return;
    }

    writer_line(&ctx->writer, "{");
    writer_push(&ctx->writer);
    cg_scope_push(ctx);
    for (size_t i = 0; i < fn->params.count; i++) {
        ASTFunctionParam *param = fn->params.items[i];
        cg_scope_add(ctx, param->name, param->type_name, false);
    }

    const char *ret_type = cg_c_return_type_for(ctx, fn->return_type);
    bool returns_value = strcmp(ret_type, "void") != 0;
    size_t stmt_count = fn->body->statements.count;
    ASTNode *last_stmt = stmt_count > 0 ? fn->body->statements.items[stmt_count - 1] : NULL;
    bool needs_tail_return = returns_value && (stmt_count == 0 || !last_stmt || last_stmt->kind != AST_NODE_RETURN);
    const char *tail_var = NULL;
    const char *tail_helper = NULL;

    if (needs_tail_return) {
        const char *ret_storage_type = cg_c_type_for(ctx, fn->return_type);
        tail_var = "__lz_ret";
        tail_helper = cg_assign_helper_for(ctx, fn->return_type);
        writer_line(&ctx->writer, "%s %s = {0};", ret_storage_type, tail_var);
    }

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = fn->body->statements.items[i];
        bool is_last = (i + 1 == stmt_count);
        const char *stmt_tail_var = (needs_tail_return && is_last) ? tail_var : NULL;
        const char *stmt_tail_helper = (needs_tail_return && is_last) ? tail_helper : NULL;
        cg_emit_statement(ctx, stmt, stmt_tail_var, stmt_tail_helper);
    }

    if (needs_tail_return) {
        writer_line(&ctx->writer, "return %s;", tail_var);
    }

    cg_scope_pop(ctx);
    writer_pop(&ctx->writer);
    writer_line(&ctx->writer, "}");
}

static void cg_emit_function_definitions(CodegenContext *ctx) {
    for (size_t i = 0; i < ctx->function_count; i++) {
        const CGFunctionInfo *info = &ctx->functions[i];
        cg_emit_function_signature(ctx, info, false);
        ctx->current_function = info->decl;
        cg_emit_function_body(ctx, info->decl);
        writer_blank_line(&ctx->writer);
    }
    ctx->current_function = NULL;
}

static void cg_emit_entrypoint(CodegenContext *ctx) {
    const CGFunctionInfo *main_fn = cg_find_function(ctx, "main");
    writer_line(&ctx->writer, "int main(void) {");
    writer_push(&ctx->writer);
    if (main_fn) {
        if (main_fn->decl->params.count == 0) {
            writer_line(&ctx->writer, "%s();", main_fn->c_name);
        } else {
            writer_line(&ctx->writer,
                        "/* TODO: pass CLI arguments to main */");
            writer_line(&ctx->writer, "%s();", main_fn->c_name);
        }
        writer_line(&ctx->writer, "return 0;");
    } else {
        writer_line(&ctx->writer,
                    "fprintf(stderr, \"no entry point defined\\n\");");
        writer_line(&ctx->writer, "return 1;");
    }
    writer_pop(&ctx->writer);
    writer_line(&ctx->writer, "}");
}


static void cg_emit_block(CodegenContext *ctx,
                          ASTBlock *block,
                          const char *tail_var,
                          const char *tail_helper) {
    writer_line(&ctx->writer, "{");
    writer_push(&ctx->writer);
    cg_scope_push(ctx);
    if (block) {
        for (size_t i = 0; i < block->statements.count; i++) {
            bool is_last = (i + 1 == block->statements.count);
            const char *stmt_tail_var = (tail_var && is_last) ? tail_var : NULL;
            const char *stmt_tail_helper = (tail_helper && is_last) ? tail_helper : NULL;
            cg_emit_statement(ctx,
                              block->statements.items[i],
                              stmt_tail_var,
                              stmt_tail_helper);
        }
    }
    cg_scope_pop(ctx);
    writer_pop(&ctx->writer);
    writer_line(&ctx->writer, "}");
}

static void cg_emit_statement(CodegenContext *ctx,
                              ASTNode *node,
                              const char *tail_var,
                              const char *tail_helper) {
    if (ctx->had_error || !node) {
        return;
    }
    switch (node->kind) {
        case AST_NODE_VAR_DECL:
            cg_emit_var_decl(ctx, (ASTVarDecl *)node);
            break;
        case AST_NODE_ASSIGN:
            cg_emit_assignment(ctx, (ASTAssignStmt *)node);
            break;
        case AST_NODE_IF:
            cg_emit_if(ctx, (ASTIfStmt *)node, tail_var, tail_helper);
            break;
        case AST_NODE_RETURN:
            cg_emit_return(ctx, (ASTReturnStmt *)node);
            break;
        case AST_NODE_EXPR_STMT:
            cg_emit_expr_stmt(ctx, (ASTExprStmt *)node, tail_var, tail_helper);
            break;
        case AST_NODE_FOR:
            cg_fail(ctx, &node->token, "for-in loops are not supported yet");
            break;
        default:
            cg_fail(ctx, &node->token, "unsupported statement kind in codegen");
            break;
    }
}

static void cg_emit_var_decl(CodegenContext *ctx, ASTVarDecl *decl) {
    const char *c_type = cg_c_type_for(ctx, decl->type_name);
    writer_line(&ctx->writer, "%s %s = {0};", c_type, decl->name);
    cg_scope_add(ctx, decl->name, decl->type_name, decl->is_mutable);
    cg_emit_assignment_call(ctx, decl->name, decl->type_name, decl->initializer);
}

static void cg_emit_assignment(CodegenContext *ctx, ASTAssignStmt *assign) {
    const CGVarBinding *binding = cg_scope_lookup(ctx, assign->target);
    if (!binding) {
        cg_fail(ctx, &assign->base.token, "assignment to unknown symbol");
        return;
    }
    cg_emit_assignment_call(ctx, assign->target, binding->type_name, assign->value);
}

static void cg_emit_if(CodegenContext *ctx,
                       ASTIfStmt *stmt,
                       const char *tail_var,
                       const char *tail_helper) {
    writer_begin_line(&ctx->writer);
    writer_printf(&ctx->writer, "if (");
    cg_emit_expression(ctx, stmt->condition);
    writer_printf(&ctx->writer, ") ");
    writer_end_line(&ctx->writer);
    cg_emit_block(ctx, stmt->then_block, tail_var, tail_helper);
    if (stmt->else_block) {
        writer_line(&ctx->writer, "else");
        cg_emit_block(ctx, stmt->else_block, tail_var, tail_helper);
    }
}

static void cg_emit_return(CodegenContext *ctx, ASTReturnStmt *stmt) {
    writer_begin_line(&ctx->writer);
    writer_printf(&ctx->writer, "return");
    if (stmt->value) {
        writer_printf(&ctx->writer, " ");
        cg_emit_expression(ctx, stmt->value);
    }
    writer_printf(&ctx->writer, ";");
    writer_end_line(&ctx->writer);
}

static void cg_emit_expr_stmt(CodegenContext *ctx,
                              ASTExprStmt *stmt,
                              const char *tail_var,
                              const char *tail_helper) {
    writer_begin_line(&ctx->writer);
    if (tail_var && tail_helper && stmt->expr) {
        writer_printf(&ctx->writer, "%s(&%s, ", tail_helper, tail_var);
        cg_emit_expression(ctx, stmt->expr);
        writer_printf(&ctx->writer, ");");
    } else {
        if (stmt->expr) {
            cg_emit_expression(ctx, stmt->expr);
        }
        writer_printf(&ctx->writer, ";");
    }
    writer_end_line(&ctx->writer);
}

static void cg_emit_expression(CodegenContext *ctx, ASTNode *node) {
    if (!node) {
        writer_printf(&ctx->writer, "NULL");
        return;
    }
    switch (node->kind) {
        case AST_NODE_EXPR_LITERAL:
            cg_emit_literal(ctx, (ASTLiteralExpr *)node);
            break;
        case AST_NODE_EXPR_IDENTIFIER:
            cg_emit_identifier(ctx, (ASTIdentifierExpr *)node);
            break;
        case AST_NODE_EXPR_CALL:
            cg_emit_call(ctx, (ASTCallExpr *)node);
            break;
        case AST_NODE_EXPR_BINARY:
            cg_emit_binary(ctx, (ASTBinaryExpr *)node);
            break;
        default:
            cg_fail(ctx, &node->token, "unsupported expression kind");
            writer_printf(&ctx->writer, "/* unsupported expr */");
            break;
    }
}

static void cg_emit_literal(CodegenContext *ctx, ASTLiteralExpr *literal) {
    switch (literal->literal_kind) {
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
            writer_printf(&ctx->writer, "%s", literal->text ? literal->text : "0");
            break;
        case AST_LITERAL_BOOL:
            writer_printf(&ctx->writer, literal->bool_value ? "true" : "false");
            break;
        case AST_LITERAL_STRING:
            cg_emit_string_literal(ctx, literal->text ? literal->text : "");
            break;
        case AST_LITERAL_NULL:
            writer_printf(&ctx->writer, "NULL");
            break;
    }
}

static void cg_emit_identifier(CodegenContext *ctx, ASTIdentifierExpr *ident) {
    if (strcmp(ident->name, "log") == 0) {
        writer_printf(&ctx->writer, "lz_runtime_log");
        return;
    }
    const CGVarBinding *binding = cg_scope_lookup(ctx, ident->name);
    if (binding) {
        writer_printf(&ctx->writer, "%s", ident->name);
        return;
    }
    const CGFunctionInfo *fn = cg_find_function(ctx, ident->name);
    if (fn) {
        writer_printf(&ctx->writer, "%s", fn->c_name);
        return;
    }
    writer_printf(&ctx->writer, "%s", ident->name);
}

static void cg_emit_call(CodegenContext *ctx, ASTCallExpr *call) {
    cg_emit_expression(ctx, call->callee);
    writer_printf(&ctx->writer, "(");
    for (size_t i = 0; i < call->arguments.count; i++) {
        if (i > 0) {
            writer_printf(&ctx->writer, ", ");
        }
        cg_emit_expression(ctx, call->arguments.items[i]);
    }
    writer_printf(&ctx->writer, ")");
}

static const char *cg_binary_op(TokenType type) {
    switch (type) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_EQEQ: return "==";
        case TOKEN_BANGEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_LTE: return "<=";
        case TOKEN_GT: return ">";
        case TOKEN_GTE: return ">=";
        default: return "/*?*/";
    }
}

static void cg_emit_binary(CodegenContext *ctx, ASTBinaryExpr *binary) {
    writer_printf(&ctx->writer, "(");
    cg_emit_expression(ctx, binary->left);
    writer_printf(&ctx->writer, " %s ", cg_binary_op(binary->op));
    cg_emit_expression(ctx, binary->right);
    writer_printf(&ctx->writer, ")");
}

static void cg_emit_string_literal(CodegenContext *ctx, const char *text) {
    writer_printf(&ctx->writer, "lz_string_from_literal(\"");
    if (text) {
        for (const char *c = text; *c; c++) {
            unsigned char ch = (unsigned char)*c;
            switch (ch) {
                case '\\': writer_printf(&ctx->writer, "\\\\"); break;
                case '"': writer_printf(&ctx->writer, "\\\""); break;
                case '\n': writer_printf(&ctx->writer, "\\n"); break;
                case '\r': writer_printf(&ctx->writer, "\\r"); break;
                case '\t': writer_printf(&ctx->writer, "\\t"); break;
                default:
                    if (isprint(ch)) {
                        fputc(ch, ctx->writer.file);
                    } else {
                        writer_printf(&ctx->writer, "\\x%02X", ch);
                    }
                    break;
            }
        }
    }
    writer_printf(&ctx->writer, "\")");
}

static const char *cg_c_type_for(const CodegenContext *ctx, const char *type_name) {
    if (!type_name) {
        return "void *";
    }
    if (strcmp(type_name, "int") == 0) {
        return "int64_t";
    }
    if (strcmp(type_name, "float") == 0) {
        return "double";
    }
    if (strcmp(type_name, "bool") == 0) {
        return "bool";
    }
    if (strcmp(type_name, "string") == 0) {
        return "struct lz_string *";
    }
    if (strcmp(type_name, "null") == 0) {
        return "void *";
    }
    if (cg_type_is_result(type_name)) {
        return "lz_result";
    }
    if (cg_type_is_maybe(type_name)) {
        return "lz_maybe";
    }
    if (cg_type_is_struct(ctx, type_name)) {
        return type_name;
    }
    return type_name;
}

static const char *cg_c_return_type_for(const CodegenContext *ctx, const char *type_name) {
    if (!type_name || strcmp(type_name, "null") == 0) {
        return "void";
    }
    return cg_c_type_for(ctx, type_name);
}

static const char *cg_assign_helper_for(const CodegenContext *ctx, const char *type_name) {
    if (!type_name) {
        return "lz_assign_ptr";
    }
    if (strcmp(type_name, "int") == 0) {
        return "lz_assign_int64";
    }
    if (strcmp(type_name, "float") == 0) {
        return "lz_assign_double";
    }
    if (strcmp(type_name, "bool") == 0) {
        return "lz_assign_bool";
    }
    if (strcmp(type_name, "string") == 0) {
        return "lz_assign_string";
    }
    if (cg_type_is_result(type_name)) {
        return "lz_assign_result";
    }
    if (cg_type_is_maybe(type_name)) {
        return "lz_assign_maybe";
    }
    const CGStructInfo *info = cg_find_struct(ctx, type_name);
    if (info) {
        return info->assign_helper;
    }
    return "lz_assign_ptr";
}

static bool cg_type_is_result(const char *type_name) {
    if (!type_name) return false;
    return strncmp(type_name, "result", strlen("result")) == 0;
}

static bool cg_type_is_maybe(const char *type_name) {
    if (!type_name) return false;
    return strncmp(type_name, "maybe", strlen("maybe")) == 0;
}

static bool cg_type_is_struct(const CodegenContext *ctx, const char *type_name) {
    return cg_find_struct(ctx, type_name) != NULL;
}

static bool cg_fail(CodegenContext *ctx, const Token *token, const char *message) {
    if (ctx->had_error) {
        return false;
    }
    if (token) {
        fprintf(stderr,
                "[line %d:%d] Codegen error: %s\n",
                token->line,
                token->column,
                message);
    } else {
        fprintf(stderr, "Codegen error: %s\n", message);
    }
    ctx->had_error = true;
    return false;
}

static void cg_emit_assignment_call(CodegenContext *ctx,
                                    const char *target_name,
                                    const char *type_name,
                                    ASTNode *value) {
    writer_begin_line(&ctx->writer);
    writer_printf(&ctx->writer, "%s(&%s, ", cg_assign_helper_for(ctx, type_name), target_name);
    cg_emit_expression(ctx, value);
    writer_printf(&ctx->writer, ");");
    writer_end_line(&ctx->writer);
}

static bool cg_command_exists(const char *cmd) {
    char command[256];
    snprintf(command, sizeof(command), "command -v %s >/dev/null 2>&1", cmd);
    int result = system(command);
    return result == 0;
}

static bool cg_invoke_compiler(const char *compiler,
                               const char *c_path,
                               const char *binary_path) {
    char command[1024];
    const char *runtime_path = "src/runtime/runtime.c";
    snprintf(command,
             sizeof(command),
             "%s -std=c11 -Wall -Wextra \"%s\" \"%s\" -o \"%s\"",
             compiler,
             c_path,
             runtime_path,
             binary_path);
    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "%s failed while building '%s'\n", compiler, binary_path);
        return false;
    }
    return true;
}

static bool cg_run_clang(const char *c_path, const char *binary_path) {
    if (cg_command_exists("clang")) {
        return cg_invoke_compiler("clang", c_path, binary_path);
    }
    fprintf(stderr, "clang not found; attempting to use cc instead\n");
    if (cg_command_exists("cc")) {
        return cg_invoke_compiler("cc", c_path, binary_path);
    }
    fprintf(stderr, "no suitable C compiler found (missing clang and cc)\n");
    return false;
}
