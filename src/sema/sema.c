#include "sema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    FLOW_MODE_NONE,
    FLOW_MODE_MAYBE,
    FLOW_MODE_RESULT,
} FlowMode;

typedef struct {
    const char *name;
    bool is_mutable;
    const char *type_name;
    Token token;
} VarSymbol;

typedef struct {
    VarSymbol *items;
    size_t count;
    size_t capacity;
} VarScope;

typedef struct {
    const char *name;
    const char *return_type;
    const ASTFunctionDecl *decl;
    Token token;
} FunctionSymbol;

typedef struct {
    VarScope *scopes;
    size_t scope_count;
    size_t scope_capacity;

    FunctionSymbol *functions;
    size_t function_count;
    size_t function_capacity;

    const ASTFunctionDecl *current_function;
    FlowMode current_flow_mode;
} SemaContext;

static void sema_context_init(SemaContext *ctx);
static void sema_context_destroy(SemaContext *ctx);
static void sema_push_scope(SemaContext *ctx);
static void sema_pop_scope(SemaContext *ctx);
static void sema_add_var(SemaContext *ctx,
                         const char *name,
                         bool is_mutable,
                         const char *type_name,
                         Token token);
static VarSymbol *sema_lookup_var(SemaContext *ctx, const char *name);
static void sema_register_function(SemaContext *ctx, ASTFunctionDecl *fn);
static void sema_add_function_symbol(SemaContext *ctx,
                                     const char *name,
                                     const char *return_type,
                                     const ASTFunctionDecl *decl,
                                     Token token);
static void sema_register_builtins(SemaContext *ctx);
static const FunctionSymbol *sema_lookup_function(SemaContext *ctx, const char *name);
static void sema_note_flow_usage(SemaContext *ctx, FlowMode mode, Token token);
static FlowMode flow_mode_from_type(const char *type_name);
static bool type_is_maybe(const char *type_name);
static bool type_is_result(const char *type_name);
static void sema_error(Token token, const char *message);

static void sema_check_declaration(SemaContext *ctx, ASTNode *node);
static void sema_check_function(SemaContext *ctx, ASTFunctionDecl *fn);
static void sema_check_struct(SemaContext *ctx, ASTStructDecl *decl);
static void sema_check_block(SemaContext *ctx, ASTBlock *block, bool owns_scope);
static void sema_check_statement(SemaContext *ctx, ASTNode *node);
static void sema_check_expression(SemaContext *ctx, ASTNode *node);
static void sema_check_unused_result(SemaContext *ctx, ASTExprStmt *stmt);

void sema_check_program(ASTProgram *program) {
    SemaContext ctx;
    sema_context_init(&ctx);
    sema_register_builtins(&ctx);

    for (size_t i = 0; i < program->declarations.count; i++) {
        ASTNode *node = program->declarations.items[i];
        if (node->kind == AST_NODE_FUNCTION) {
            sema_register_function(&ctx, (ASTFunctionDecl *)node);
        }
    }

    for (size_t i = 0; i < program->declarations.count; i++) {
        sema_check_declaration(&ctx, program->declarations.items[i]);
    }

    sema_context_destroy(&ctx);
}

static void sema_context_init(SemaContext *ctx) {
    ctx->scopes = NULL;
    ctx->scope_count = 0;
    ctx->scope_capacity = 0;
    ctx->functions = NULL;
    ctx->function_count = 0;
    ctx->function_capacity = 0;
    ctx->current_function = NULL;
    ctx->current_flow_mode = FLOW_MODE_NONE;
}

static void sema_context_destroy(SemaContext *ctx) {
    for (size_t i = 0; i < ctx->scope_count; i++) {
        free(ctx->scopes[i].items);
    }
    free(ctx->scopes);
    free(ctx->functions);
}

static void sema_push_scope(SemaContext *ctx) {
    if (ctx->scope_count == ctx->scope_capacity) {
        size_t new_capacity = ctx->scope_capacity ? ctx->scope_capacity * 2 : 4;
        VarScope *new_scopes = realloc(ctx->scopes, new_capacity * sizeof(VarScope));
        if (!new_scopes) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        ctx->scopes = new_scopes;
        ctx->scope_capacity = new_capacity;
    }
    VarScope *scope = &ctx->scopes[ctx->scope_count++];
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static void sema_pop_scope(SemaContext *ctx) {
    if (ctx->scope_count == 0) return;
    VarScope *scope = &ctx->scopes[--ctx->scope_count];
    free(scope->items);
    scope->items = NULL;
    scope->count = 0;
    scope->capacity = 0;
}

static void sema_add_var(SemaContext *ctx,
                         const char *name,
                         bool is_mutable,
                         const char *type_name,
                         Token token) {
    if (ctx->scope_count == 0) {
        sema_push_scope(ctx);
    }
    VarScope *scope = &ctx->scopes[ctx->scope_count - 1];
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->items[i].name, name) == 0) {
            sema_error(token, "symbol already declared in this scope");
        }
    }
    if (scope->count == scope->capacity) {
        size_t new_capacity = scope->capacity ? scope->capacity * 2 : 4;
        VarSymbol *new_items = realloc(scope->items, new_capacity * sizeof(VarSymbol));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        scope->items = new_items;
        scope->capacity = new_capacity;
    }
    scope->items[scope->count++] = (VarSymbol){
        .name = name,
        .is_mutable = is_mutable,
        .type_name = type_name,
        .token = token,
    };
}

static VarSymbol *sema_lookup_var(SemaContext *ctx, const char *name) {
    for (size_t s = ctx->scope_count; s > 0; s--) {
        VarScope *scope = &ctx->scopes[s - 1];
        for (size_t i = 0; i < scope->count; i++) {
            if (strcmp(scope->items[i].name, name) == 0) {
                return &scope->items[i];
            }
        }
    }
    return NULL;
}

static void sema_register_function(SemaContext *ctx, ASTFunctionDecl *fn) {
    sema_add_function_symbol(ctx, fn->name, fn->return_type, fn, fn->base.token);
}

static void sema_add_function_symbol(SemaContext *ctx,
                                     const char *name,
                                     const char *return_type,
                                     const ASTFunctionDecl *decl,
                                     Token token) {
    for (size_t i = 0; i < ctx->function_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0) {
            sema_error(token, "function already declared");
        }
    }
    if (ctx->function_count == ctx->function_capacity) {
        size_t new_capacity = ctx->function_capacity ? ctx->function_capacity * 2 : 4;
        FunctionSymbol *new_items = realloc(ctx->functions, new_capacity * sizeof(FunctionSymbol));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        ctx->functions = new_items;
        ctx->function_capacity = new_capacity;
    }
    ctx->functions[ctx->function_count++] = (FunctionSymbol){
        .name = name,
        .return_type = return_type,
        .decl = decl,
        .token = token,
    };
}

static void sema_register_builtins(SemaContext *ctx) {
    static const struct {
        const char *name;
        const char *return_type;
    } builtins[] = {
        { "log", "null" },
    };

    Token token = {
        .lexeme = "",
        .length = 0,
        .line = 0,
        .column = 0,
        .type = TOKEN_IDENT,
    };

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        sema_add_function_symbol(ctx,
                                 builtins[i].name,
                                 builtins[i].return_type,
                                 NULL,
                                 token);
    }
}

static const FunctionSymbol *sema_lookup_function(SemaContext *ctx, const char *name) {
    for (size_t i = 0; i < ctx->function_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0) {
            return &ctx->functions[i];
        }
    }
    return NULL;
}

static void sema_note_flow_usage(SemaContext *ctx, FlowMode mode, Token token) {
    if (mode == FLOW_MODE_NONE) {
        return;
    }
    if (ctx->current_flow_mode == FLOW_MODE_NONE) {
        ctx->current_flow_mode = mode;
        return;
    }
    if (ctx->current_flow_mode != mode) {
        sema_error(token, "cannot mix maybe and result in the same function");
    }
}

static FlowMode flow_mode_from_type(const char *type_name) {
    if (type_is_result(type_name)) {
        return FLOW_MODE_RESULT;
    }
    if (type_is_maybe(type_name)) {
        return FLOW_MODE_MAYBE;
    }
    return FLOW_MODE_NONE;
}

static bool type_starts_with(const char *type_name, const char *prefix) {
    if (!type_name) return false;
    size_t len = strlen(prefix);
    if (strncmp(type_name, prefix, len) != 0) return false;
    char next = type_name[len];
    return next == '\0' || next == '[';
}

static bool type_is_maybe(const char *type_name) {
    return type_starts_with(type_name, "maybe");
}

static bool type_is_result(const char *type_name) {
    return type_starts_with(type_name, "result");
}

static void sema_error(Token token, const char *message) {
    fprintf(stderr, "[line %d:%d] Semantic error: %s\n", token.line, token.column, message);
    exit(EXIT_FAILURE);
}

static void sema_check_declaration(SemaContext *ctx, ASTNode *node) {
    switch (node->kind) {
        case AST_NODE_FUNCTION:
            sema_check_function(ctx, (ASTFunctionDecl *)node);
            break;
        case AST_NODE_STRUCT:
            sema_check_struct(ctx, (ASTStructDecl *)node);
            break;
        default:
            break;
    }
}

static void sema_check_function(SemaContext *ctx, ASTFunctionDecl *fn) {
    const ASTFunctionDecl *prev_function = ctx->current_function;
    FlowMode previous_flow = ctx->current_flow_mode;

    ctx->current_function = fn;
    ctx->current_flow_mode = flow_mode_from_type(fn->return_type);

    sema_push_scope(ctx);
    for (size_t i = 0; i < fn->params.count; i++) {
        ASTFunctionParam *param = fn->params.items[i];
        sema_note_flow_usage(ctx, flow_mode_from_type(param->type_name), param->token);
        sema_add_var(ctx, param->name, false, param->type_name, param->token);
    }
    sema_check_block(ctx, fn->body, false);
    sema_pop_scope(ctx);

    ctx->current_function = prev_function;
    ctx->current_flow_mode = previous_flow;
}

static void sema_check_struct(SemaContext *ctx, ASTStructDecl *decl) {
    (void)ctx;
    for (size_t i = 0; i < decl->fields.count; i++) {
        ASTStructField *field_i = decl->fields.items[i];
        for (size_t j = i + 1; j < decl->fields.count; j++) {
            ASTStructField *field_j = decl->fields.items[j];
            if (strcmp(field_i->name, field_j->name) == 0) {
                sema_error(field_j->token, "duplicate field name in struct");
            }
        }
    }
}

static void sema_check_block(SemaContext *ctx, ASTBlock *block, bool owns_scope) {
    if (!block) return;
    if (owns_scope) {
        sema_push_scope(ctx);
    }
    for (size_t i = 0; i < block->statements.count; i++) {
        sema_check_statement(ctx, block->statements.items[i]);
    }
    if (owns_scope) {
        sema_pop_scope(ctx);
    }
}

static void sema_check_statement(SemaContext *ctx, ASTNode *node) {
    switch (node->kind) {
        case AST_NODE_VAR_DECL: {
            ASTVarDecl *decl = (ASTVarDecl *)node;
            sema_note_flow_usage(ctx, flow_mode_from_type(decl->type_name), decl->base.token);
            sema_add_var(ctx, decl->name, decl->is_mutable, decl->type_name, decl->base.token);
            sema_check_expression(ctx, decl->initializer);
            break;
        }
        case AST_NODE_ASSIGN: {
            ASTAssignStmt *assign = (ASTAssignStmt *)node;
            VarSymbol *symbol = sema_lookup_var(ctx, assign->target);
            if (!symbol) {
                sema_error(assign->base.token, "assignment to undeclared variable");
            }
            if (!symbol->is_mutable) {
                sema_error(assign->base.token, "cannot assign to immutable variable");
            }
            sema_check_expression(ctx, assign->value);
            break;
        }
        case AST_NODE_IF: {
            ASTIfStmt *stmt = (ASTIfStmt *)node;
            sema_check_expression(ctx, stmt->condition);
            sema_check_block(ctx, stmt->then_block, true);
            sema_check_block(ctx, stmt->else_block, true);
            break;
        }
        case AST_NODE_FOR: {
            ASTForStmt *stmt = (ASTForStmt *)node;
            sema_check_expression(ctx, stmt->iterable);
            sema_push_scope(ctx);
            sema_add_var(ctx, stmt->iterator, false, NULL, stmt->base.token);
            sema_check_block(ctx, stmt->body, false);
            sema_pop_scope(ctx);
            break;
        }
        case AST_NODE_RETURN: {
            if (!ctx->current_function) {
                sema_error(node->token, "return outside of function");
            }
            ASTReturnStmt *stmt = (ASTReturnStmt *)node;
            sema_check_expression(ctx, stmt->value);
            break;
        }
        case AST_NODE_EXPR_STMT: {
            ASTExprStmt *stmt = (ASTExprStmt *)node;
            sema_check_expression(ctx, stmt->expr);
            sema_check_unused_result(ctx, stmt);
            break;
        }
        default:
            break;
    }
}

static void sema_check_expression(SemaContext *ctx, ASTNode *node) {
    if (!node) return;
    switch (node->kind) {
        case AST_NODE_EXPR_LITERAL:
            break;
        case AST_NODE_EXPR_IDENTIFIER: {
            ASTIdentifierExpr *ident = (ASTIdentifierExpr *)node;
            if (sema_lookup_var(ctx, ident->name)) {
                break;
            }
            if (!sema_lookup_function(ctx, ident->name)) {
                sema_error(node->token, "undeclared identifier");
            }
            break;
        }
        case AST_NODE_EXPR_CALL: {
            ASTCallExpr *call = (ASTCallExpr *)node;
            if (call->callee->kind == AST_NODE_EXPR_IDENTIFIER) {
                ASTIdentifierExpr *ident = (ASTIdentifierExpr *)call->callee;
                if (!sema_lookup_function(ctx, ident->name)) {
                    VarSymbol *symbol = sema_lookup_var(ctx, ident->name);
                    if (!symbol) {
                        sema_error(call->callee->token, "call to undefined function");
                    }
                }
            } else {
                sema_check_expression(ctx, call->callee);
            }
            for (size_t i = 0; i < call->arguments.count; i++) {
                sema_check_expression(ctx, call->arguments.items[i]);
            }
            break;
        }
        case AST_NODE_EXPR_BINARY: {
            ASTBinaryExpr *binary = (ASTBinaryExpr *)node;
            sema_check_expression(ctx, binary->left);
            sema_check_expression(ctx, binary->right);
            break;
        }
        default:
            break;
    }
}

static void sema_check_unused_result(SemaContext *ctx, ASTExprStmt *stmt) {
    if (!stmt->expr || stmt->expr->kind != AST_NODE_EXPR_CALL) {
        return;
    }
    ASTCallExpr *call = (ASTCallExpr *)stmt->expr;
    if (call->callee->kind != AST_NODE_EXPR_IDENTIFIER) {
        return;
    }
    ASTIdentifierExpr *ident = (ASTIdentifierExpr *)call->callee;
    const FunctionSymbol *fn = sema_lookup_function(ctx, ident->name);
    if (fn && type_is_result(fn->return_type)) {
        sema_error(call->base.token, "result-returning function must not be ignored");
    }
}
