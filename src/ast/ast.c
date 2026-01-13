#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *ast_xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static ASTNode *ast_alloc_node(size_t size, ASTNodeKind kind, const Token *token) {
    ASTNode *node = ast_xcalloc(1, size);
    node->kind = kind;
    if (token) {
        node->token = *token;
    } else {
        node->token.lexeme = "";
        node->token.length = 0;
        node->token.line = 0;
        node->token.column = 0;
        node->token.type = TOKEN_EOF;
    }
    return node;
}

void ast_array_init(ASTArray *array) {
    array->items = NULL;
    array->count = 0;
    array->capacity = 0;
}

void ast_array_append(ASTArray *array, void *item) {
    if (array->count + 1 > array->capacity) {
        size_t new_capacity = array->capacity ? array->capacity * 2 : 4;
        void **new_items = realloc(array->items, new_capacity * sizeof(void *));
        if (!new_items) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        array->items = new_items;
        array->capacity = new_capacity;
    }
    array->items[array->count++] = item;
}

void ast_array_free(ASTArray *array) {
    free(array->items);
    array->items = NULL;
    array->count = 0;
    array->capacity = 0;
}

char *ast_copy_text(const char *source, size_t length) {
    char *buffer = ast_xcalloc(length + 1, sizeof(char));
    memcpy(buffer, source, length);
    buffer[length] = '\0';
    return buffer;
}

char *ast_copy_token_text(const Token *token) {
    return ast_copy_text(token->lexeme, token->length);
}

static void ast_free_string_array(ASTArray *array) {
    for (size_t i = 0; i < array->count; i++) {
        free(array->items[i]);
    }
    ast_array_free(array);
}

static void ast_function_param_free(void *ptr) {
    if (!ptr) return;
    ASTFunctionParam *param = ptr;
    free(param->name);
    free(param->type_name);
    free(param);
}

static void ast_struct_field_free(void *ptr) {
    if (!ptr) return;
    ASTStructField *field = ptr;
    free(field->name);
    free(field->type_name);
    free(field);
}

ASTProgram *ast_program_create(void) {
    ASTProgram *program = (ASTProgram *)ast_alloc_node(sizeof(ASTProgram),
                                                       AST_NODE_PROGRAM,
                                                       NULL);
    ast_array_init(&program->imports);
    ast_array_init(&program->declarations);
    return program;
}

void ast_program_add_import(ASTProgram *program, ASTImport *import_stmt) {
    ast_array_append(&program->imports, import_stmt);
}

void ast_program_add_declaration(ASTProgram *program, ASTNode *declaration) {
    ast_array_append(&program->declarations, declaration);
}

void ast_program_destroy(ASTProgram *program) {
    if (!program) return;
    for (size_t i = 0; i < program->imports.count; i++) {
        ast_import_destroy(program->imports.items[i]);
    }
    for (size_t i = 0; i < program->declarations.count; i++) {
        ast_node_destroy(program->declarations.items[i]);
    }
    ast_array_free(&program->imports);
    ast_array_free(&program->declarations);
    free(program);
}

ASTImport *ast_import_create(const Token *import_token) {
    ASTImport *import_stmt = (ASTImport *)ast_alloc_node(sizeof(ASTImport),
                                                         AST_NODE_IMPORT,
                                                         import_token);
    ast_array_init(&import_stmt->segments);
    return import_stmt;
}

void ast_import_add_segment(ASTImport *import_stmt, const Token *segment_token) {
    ast_array_append(&import_stmt->segments,
                     ast_copy_text(segment_token->lexeme, segment_token->length));
}

void ast_import_destroy(ASTImport *import_stmt) {
    if (!import_stmt) return;
    ast_free_string_array(&import_stmt->segments);
    free(import_stmt);
}

ASTFunctionDecl *ast_function_decl_create(bool is_public, const Token *name_token) {
    ASTFunctionDecl *fn = (ASTFunctionDecl *)ast_alloc_node(sizeof(ASTFunctionDecl),
                                                            AST_NODE_FUNCTION,
                                                            name_token);
    fn->is_public = is_public;
    fn->name = ast_copy_token_text(name_token);
    fn->return_type = NULL;
    fn->body = NULL;
    ast_array_init(&fn->params);
    return fn;
}

void ast_function_decl_add_param(ASTFunctionDecl *fn,
                                 const char *type_name,
                                 size_t type_length,
                                 const Token *name_token) {
    ASTFunctionParam *param = ast_xcalloc(1, sizeof(ASTFunctionParam));
    param->name = ast_copy_token_text(name_token);
    param->type_name = ast_copy_text(type_name, type_length);
    param->token = *name_token;
    ast_array_append(&fn->params, param);
}

void ast_function_decl_set_return_type(ASTFunctionDecl *fn,
                                       const char *type_name,
                                       size_t type_length) {
    free(fn->return_type);
    fn->return_type = ast_copy_text(type_name, type_length);
}

void ast_function_decl_set_body(ASTFunctionDecl *fn, ASTBlock *body) {
    fn->body = body;
}

void ast_function_decl_destroy(ASTFunctionDecl *fn) {
    if (!fn) return;
    free(fn->name);
    free(fn->return_type);
    for (size_t i = 0; i < fn->params.count; i++) {
        ast_function_param_free(fn->params.items[i]);
    }
    ast_array_free(&fn->params);
    ast_block_destroy(fn->body);
    free(fn);
}

ASTStructDecl *ast_struct_decl_create(bool is_public, const Token *name_token) {
    ASTStructDecl *decl = (ASTStructDecl *)ast_alloc_node(sizeof(ASTStructDecl),
                                                          AST_NODE_STRUCT,
                                                          name_token);
    decl->is_public = is_public;
    decl->name = ast_copy_token_text(name_token);
    ast_array_init(&decl->fields);
    return decl;
}

void ast_struct_decl_add_field(ASTStructDecl *decl,
                               const Token *name_token,
                               const char *type_name,
                               size_t type_length) {
    ASTStructField *field = ast_xcalloc(1, sizeof(ASTStructField));
    field->name = ast_copy_token_text(name_token);
    field->type_name = ast_copy_text(type_name, type_length);
    field->token = *name_token;
    ast_array_append(&decl->fields, field);
}

void ast_struct_decl_destroy(ASTStructDecl *decl) {
    if (!decl) return;
    free(decl->name);
    for (size_t i = 0; i < decl->fields.count; i++) {
        ast_struct_field_free(decl->fields.items[i]);
    }
    ast_array_free(&decl->fields);
    free(decl);
}

ASTBlock *ast_block_create(const Token *start_token) {
    ASTBlock *block = (ASTBlock *)ast_alloc_node(sizeof(ASTBlock),
                                                 AST_NODE_BLOCK,
                                                 start_token);
    ast_array_init(&block->statements);
    return block;
}

void ast_block_add_statement(ASTBlock *block, ASTNode *statement) {
    ast_array_append(&block->statements, statement);
}

void ast_block_destroy(ASTBlock *block) {
    if (!block) return;
    for (size_t i = 0; i < block->statements.count; i++) {
        ast_node_destroy(block->statements.items[i]);
    }
    ast_array_free(&block->statements);
    free(block);
}

ASTVarDecl *ast_var_decl_create(const Token *name_token, bool is_mutable) {
    ASTVarDecl *decl = (ASTVarDecl *)ast_alloc_node(sizeof(ASTVarDecl),
                                                    AST_NODE_VAR_DECL,
                                                    name_token);
    decl->is_mutable = is_mutable;
    decl->name = ast_copy_token_text(name_token);
    decl->type_name = NULL;
    decl->initializer = NULL;
    return decl;
}

void ast_var_decl_set_type(ASTVarDecl *decl, const char *type_name, size_t type_length) {
    free(decl->type_name);
    decl->type_name = ast_copy_text(type_name, type_length);
}

void ast_var_decl_set_initializer(ASTVarDecl *decl, ASTNode *expr) {
    decl->initializer = expr;
}

void ast_var_decl_destroy(ASTVarDecl *decl) {
    if (!decl) return;
    free(decl->name);
    free(decl->type_name);
    ast_node_destroy(decl->initializer);
    free(decl);
}

ASTAssignStmt *ast_assign_create(const Token *name_token, ASTNode *value) {
    ASTAssignStmt *assign_stmt = (ASTAssignStmt *)ast_alloc_node(sizeof(ASTAssignStmt),
                                                                 AST_NODE_ASSIGN,
                                                                 name_token);
    assign_stmt->target = ast_copy_token_text(name_token);
    assign_stmt->value = value;
    return assign_stmt;
}

void ast_assign_destroy(ASTAssignStmt *assign_stmt) {
    if (!assign_stmt) return;
    free(assign_stmt->target);
    ast_node_destroy(assign_stmt->value);
    free(assign_stmt);
}

ASTIfStmt *ast_if_create(const Token *if_token,
                         ASTNode *condition,
                         ASTBlock *then_block,
                         ASTBlock *else_block) {
    ASTIfStmt *stmt = (ASTIfStmt *)ast_alloc_node(sizeof(ASTIfStmt),
                                                  AST_NODE_IF,
                                                  if_token);
    stmt->condition = condition;
    stmt->then_block = then_block;
    stmt->else_block = else_block;
    return stmt;
}

void ast_if_destroy(ASTIfStmt *stmt) {
    if (!stmt) return;
    ast_node_destroy(stmt->condition);
    ast_block_destroy(stmt->then_block);
    ast_block_destroy(stmt->else_block);
    free(stmt);
}

ASTForStmt *ast_for_create(const Token *for_token,
                           const Token *iterator_token,
                           ASTNode *iterable,
                           ASTBlock *body) {
    ASTForStmt *stmt = (ASTForStmt *)ast_alloc_node(sizeof(ASTForStmt),
                                                    AST_NODE_FOR,
                                                    for_token);
    stmt->iterator = ast_copy_token_text(iterator_token);
    stmt->iterable = iterable;
    stmt->body = body;
    return stmt;
}

void ast_for_destroy(ASTForStmt *stmt) {
    if (!stmt) return;
    free(stmt->iterator);
    ast_node_destroy(stmt->iterable);
    ast_block_destroy(stmt->body);
    free(stmt);
}

ASTReturnStmt *ast_return_create(const Token *return_token, ASTNode *value) {
    ASTReturnStmt *stmt = (ASTReturnStmt *)ast_alloc_node(sizeof(ASTReturnStmt),
                                                          AST_NODE_RETURN,
                                                          return_token);
    stmt->value = value;
    return stmt;
}

void ast_return_destroy(ASTReturnStmt *stmt) {
    if (!stmt) return;
    ast_node_destroy(stmt->value);
    free(stmt);
}

ASTExprStmt *ast_expr_stmt_create(ASTNode *expr) {
    ASTExprStmt *stmt = (ASTExprStmt *)ast_alloc_node(sizeof(ASTExprStmt),
                                                      AST_NODE_EXPR_STMT,
                                                      expr ? &expr->token : NULL);
    stmt->expr = expr;
    return stmt;
}

void ast_expr_stmt_destroy(ASTExprStmt *stmt) {
    if (!stmt) return;
    ast_node_destroy(stmt->expr);
    free(stmt);
}

ASTLiteralExpr *ast_literal_create(const Token *token, ASTLiteralKind kind) {
    ASTLiteralExpr *literal = (ASTLiteralExpr *)ast_alloc_node(sizeof(ASTLiteralExpr),
                                                               AST_NODE_EXPR_LITERAL,
                                                               token);
    literal->literal_kind = kind;
    literal->text = NULL;
    literal->bool_value = false;
    return literal;
}

void ast_literal_set_text(ASTLiteralExpr *literal, const char *text, size_t length) {
    free(literal->text);
    literal->text = ast_copy_text(text, length);
}

void ast_literal_set_bool(ASTLiteralExpr *literal, bool value) {
    literal->bool_value = value;
}

void ast_literal_destroy(ASTLiteralExpr *literal) {
    if (!literal) return;
    free(literal->text);
    free(literal);
}

ASTIdentifierExpr *ast_identifier_create(const Token *name_token) {
    ASTIdentifierExpr *ident = (ASTIdentifierExpr *)ast_alloc_node(sizeof(ASTIdentifierExpr),
                                                                   AST_NODE_EXPR_IDENTIFIER,
                                                                   name_token);
    ident->name = ast_copy_token_text(name_token);
    return ident;
}

void ast_identifier_destroy(ASTIdentifierExpr *ident) {
    if (!ident) return;
    free(ident->name);
    free(ident);
}

ASTCallExpr *ast_call_create(ASTNode *callee, const Token *call_token) {
    ASTCallExpr *call_expr = (ASTCallExpr *)ast_alloc_node(sizeof(ASTCallExpr),
                                                           AST_NODE_EXPR_CALL,
                                                           call_token ? call_token : (callee ? &callee->token : NULL));
    call_expr->callee = callee;
    ast_array_init(&call_expr->arguments);
    return call_expr;
}

void ast_call_add_argument(ASTCallExpr *call_expr, ASTNode *argument) {
    ast_array_append(&call_expr->arguments, argument);
}

void ast_call_destroy(ASTCallExpr *call_expr) {
    if (!call_expr) return;
    ast_node_destroy(call_expr->callee);
    for (size_t i = 0; i < call_expr->arguments.count; i++) {
        ast_node_destroy(call_expr->arguments.items[i]);
    }
    ast_array_free(&call_expr->arguments);
    free(call_expr);
}

ASTBinaryExpr *ast_binary_create(ASTNode *left,
                                 TokenType op,
                                 ASTNode *right,
                                 const Token *op_token) {
    ASTBinaryExpr *expr = (ASTBinaryExpr *)ast_alloc_node(sizeof(ASTBinaryExpr),
                                                          AST_NODE_EXPR_BINARY,
                                                          op_token);
    expr->left = left;
    expr->right = right;
    expr->op = op;
    return expr;
}

void ast_binary_destroy(ASTBinaryExpr *binary_expr) {
    if (!binary_expr) return;
    ast_node_destroy(binary_expr->left);
    ast_node_destroy(binary_expr->right);
    free(binary_expr);
}

void ast_node_destroy(ASTNode *node) {
    if (!node) return;
    switch (node->kind) {
        case AST_NODE_PROGRAM:
            ast_program_destroy((ASTProgram *)node);
            break;
        case AST_NODE_IMPORT:
            ast_import_destroy((ASTImport *)node);
            break;
        case AST_NODE_FUNCTION:
            ast_function_decl_destroy((ASTFunctionDecl *)node);
            break;
        case AST_NODE_STRUCT:
            ast_struct_decl_destroy((ASTStructDecl *)node);
            break;
        case AST_NODE_BLOCK:
            ast_block_destroy((ASTBlock *)node);
            break;
        case AST_NODE_VAR_DECL:
            ast_var_decl_destroy((ASTVarDecl *)node);
            break;
        case AST_NODE_ASSIGN:
            ast_assign_destroy((ASTAssignStmt *)node);
            break;
        case AST_NODE_IF:
            ast_if_destroy((ASTIfStmt *)node);
            break;
        case AST_NODE_FOR:
            ast_for_destroy((ASTForStmt *)node);
            break;
        case AST_NODE_RETURN:
            ast_return_destroy((ASTReturnStmt *)node);
            break;
        case AST_NODE_EXPR_STMT:
            ast_expr_stmt_destroy((ASTExprStmt *)node);
            break;
        case AST_NODE_EXPR_LITERAL:
            ast_literal_destroy((ASTLiteralExpr *)node);
            break;
        case AST_NODE_EXPR_IDENTIFIER:
            ast_identifier_destroy((ASTIdentifierExpr *)node);
            break;
        case AST_NODE_EXPR_CALL:
            ast_call_destroy((ASTCallExpr *)node);
            break;
        case AST_NODE_EXPR_BINARY:
            ast_binary_destroy((ASTBinaryExpr *)node);
            break;
    }
}
