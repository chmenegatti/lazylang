#ifndef LZ_AST_H
#define LZ_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "../lexer.h"

typedef enum {
    AST_NODE_PROGRAM,
    AST_NODE_IMPORT,
    AST_NODE_FUNCTION,
    AST_NODE_STRUCT,
    AST_NODE_BLOCK,
    AST_NODE_VAR_DECL,
    AST_NODE_ASSIGN,
    AST_NODE_IF,
    AST_NODE_FOR,
    AST_NODE_RETURN,
    AST_NODE_EXPR_STMT,
    AST_NODE_EXPR_LITERAL,
    AST_NODE_EXPR_IDENTIFIER,
    AST_NODE_EXPR_CALL,
    AST_NODE_EXPR_BINARY
} ASTNodeKind;

typedef enum {
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_NULL
} ASTLiteralKind;

typedef struct {
    void **items;
    size_t count;
    size_t capacity;
} ASTArray;

typedef struct ASTNode ASTNode;
typedef struct ASTProgram ASTProgram;
typedef struct ASTImport ASTImport;
typedef struct ASTFunctionDecl ASTFunctionDecl;
typedef struct ASTFunctionParam ASTFunctionParam;
typedef struct ASTStructDecl ASTStructDecl;
typedef struct ASTStructField ASTStructField;
typedef struct ASTBlock ASTBlock;
typedef struct ASTVarDecl ASTVarDecl;
typedef struct ASTAssignStmt ASTAssignStmt;
typedef struct ASTIfStmt ASTIfStmt;
typedef struct ASTForStmt ASTForStmt;
typedef struct ASTReturnStmt ASTReturnStmt;
typedef struct ASTExprStmt ASTExprStmt;
typedef struct ASTLiteralExpr ASTLiteralExpr;
typedef struct ASTIdentifierExpr ASTIdentifierExpr;
typedef struct ASTCallExpr ASTCallExpr;
typedef struct ASTBinaryExpr ASTBinaryExpr;

struct ASTNode {
    ASTNodeKind kind;
    Token token;
};

struct ASTProgram {
    ASTNode base;
    ASTArray imports;      /* ASTImport* */
    ASTArray declarations; /* ASTNode* */
};

struct ASTImport {
    ASTNode base;
    ASTArray segments; /* char* */
};

struct ASTFunctionParam {
    char *name;
    char *type_name;
    Token token;
};

struct ASTFunctionDecl {
    ASTNode base;
    bool is_public;
    char *name;
    ASTArray params; /* ASTFunctionParam* */
    char *return_type;
    ASTBlock *body;
};

struct ASTStructField {
    char *name;
    char *type_name;
    Token token;
};

struct ASTStructDecl {
    ASTNode base;
    bool is_public;
    char *name;
    ASTArray fields; /* ASTStructField* */
};

struct ASTBlock {
    ASTNode base;
    ASTArray statements; /* ASTNode* */
};

struct ASTVarDecl {
    ASTNode base;
    bool is_mutable;
    char *name;
    char *type_name;
    ASTNode *initializer;
};

struct ASTAssignStmt {
    ASTNode base;
    char *target;
    ASTNode *value;
};

struct ASTIfStmt {
    ASTNode base;
    ASTNode *condition;
    ASTBlock *then_block;
    ASTBlock *else_block;
};

struct ASTForStmt {
    ASTNode base;
    char *iterator;
    ASTNode *iterable;
    ASTBlock *body;
};

struct ASTReturnStmt {
    ASTNode base;
    ASTNode *value; /* optional */
};

struct ASTExprStmt {
    ASTNode base;
    ASTNode *expr;
};

struct ASTLiteralExpr {
    ASTNode base;
    ASTLiteralKind literal_kind;
    char *text;
    bool bool_value;
};

struct ASTIdentifierExpr {
    ASTNode base;
    char *name;
};

struct ASTCallExpr {
    ASTNode base;
    ASTNode *callee;
    ASTArray arguments; /* ASTNode* */
};

struct ASTBinaryExpr {
    ASTNode base;
    TokenType op;
    ASTNode *left;
    ASTNode *right;
};

void ast_array_init(ASTArray *array);
void ast_array_append(ASTArray *array, void *item);
void ast_array_free(ASTArray *array);

char *ast_copy_text(const char *source, size_t length);
char *ast_copy_token_text(const Token *token);

ASTProgram *ast_program_create(void);
void ast_program_add_import(ASTProgram *program, ASTImport *import_stmt);
void ast_program_add_declaration(ASTProgram *program, ASTNode *declaration);
void ast_program_destroy(ASTProgram *program);

ASTImport *ast_import_create(const Token *import_token);
void ast_import_add_segment(ASTImport *import_stmt, const Token *segment_token);
void ast_import_destroy(ASTImport *import_stmt);

ASTFunctionDecl *ast_function_decl_create(bool is_public, const Token *name_token);
void ast_function_decl_add_param(ASTFunctionDecl *fn,
                                 const char *type_name,
                                 size_t type_length,
                                 const Token *name_token);
void ast_function_decl_set_return_type(ASTFunctionDecl *fn,
                                       const char *type_name,
                                       size_t type_length);
void ast_function_decl_set_body(ASTFunctionDecl *fn, ASTBlock *body);
void ast_function_decl_destroy(ASTFunctionDecl *fn);

ASTStructDecl *ast_struct_decl_create(bool is_public, const Token *name_token);
void ast_struct_decl_add_field(ASTStructDecl *decl,
                               const Token *name_token,
                               const char *type_name,
                               size_t type_length);
void ast_struct_decl_destroy(ASTStructDecl *decl);

ASTBlock *ast_block_create(const Token *start_token);
void ast_block_add_statement(ASTBlock *block, ASTNode *statement);
void ast_block_destroy(ASTBlock *block);

ASTVarDecl *ast_var_decl_create(const Token *name_token, bool is_mutable);
void ast_var_decl_set_type(ASTVarDecl *decl, const char *type_name, size_t type_length);
void ast_var_decl_set_initializer(ASTVarDecl *decl, ASTNode *expr);
void ast_var_decl_destroy(ASTVarDecl *decl);

ASTAssignStmt *ast_assign_create(const Token *name_token, ASTNode *value);
void ast_assign_destroy(ASTAssignStmt *assign_stmt);

ASTIfStmt *ast_if_create(const Token *if_token,
                         ASTNode *condition,
                         ASTBlock *then_block,
                         ASTBlock *else_block);
void ast_if_destroy(ASTIfStmt *stmt);

ASTForStmt *ast_for_create(const Token *for_token,
                           const Token *iterator_token,
                           ASTNode *iterable,
                           ASTBlock *body);
void ast_for_destroy(ASTForStmt *stmt);

ASTReturnStmt *ast_return_create(const Token *return_token, ASTNode *value);
void ast_return_destroy(ASTReturnStmt *stmt);

ASTExprStmt *ast_expr_stmt_create(ASTNode *expr);
void ast_expr_stmt_destroy(ASTExprStmt *stmt);

ASTLiteralExpr *ast_literal_create(const Token *token, ASTLiteralKind kind);
void ast_literal_set_text(ASTLiteralExpr *literal, const char *text, size_t length);
void ast_literal_set_bool(ASTLiteralExpr *literal, bool value);
void ast_literal_destroy(ASTLiteralExpr *literal);

ASTIdentifierExpr *ast_identifier_create(const Token *name_token);
void ast_identifier_destroy(ASTIdentifierExpr *ident);

ASTCallExpr *ast_call_create(ASTNode *callee, const Token *call_token);
void ast_call_add_argument(ASTCallExpr *call_expr, ASTNode *argument);
void ast_call_destroy(ASTCallExpr *call_expr);

ASTBinaryExpr *ast_binary_create(ASTNode *left,
                                 TokenType op,
                                 ASTNode *right,
                                 const Token *op_token);
void ast_binary_destroy(ASTBinaryExpr *binary_expr);

void ast_node_destroy(ASTNode *node);

#endif
