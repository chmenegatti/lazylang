#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Lexer *lexer;
    Token previous;
    Token current;
    Token next;
} Parser;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} TypeBuilder;

static void parser_init(Parser *parser, Lexer *lexer);
static void parser_advance(Parser *parser);
static bool parser_check(Parser *parser, TokenType type);
static bool parser_match(Parser *parser, TokenType type);
static Token parser_consume(Parser *parser, TokenType type, const char *message);
static void parser_error(Token token, const char *message);
static void parser_skip_newlines(Parser *parser);
static void parser_require_line_break(Parser *parser, const char *message);
static TokenType parser_peek_next(Parser *parser);

static void type_builder_init(TypeBuilder *builder);
static void type_builder_append(TypeBuilder *builder, const char *text, size_t length);
static char *type_builder_finish(TypeBuilder *builder);
static bool token_is_terminator(TokenType type, const TokenType *terminators, size_t count);
static char *parser_collect_type(Parser *parser,
                                 const TokenType *terminators,
                                 size_t terminator_count);

static ASTImport *parse_import(Parser *parser);
static ASTNode *parse_top_level_decl(Parser *parser);
static ASTFunctionDecl *parse_function_decl(Parser *parser, bool is_public, Token name_token);
static void parse_function_params(Parser *parser, ASTFunctionDecl *fn);
static ASTStructDecl *parse_struct_decl(Parser *parser, bool is_public);
static ASTBlock *parse_block(Parser *parser, const Token *start_token);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_if_stmt(Parser *parser);
static ASTNode *parse_for_stmt(Parser *parser);
static ASTNode *parse_var_decl(Parser *parser, bool is_mutable);
static ASTNode *parse_assignment(Parser *parser);
static ASTNode *parse_return(Parser *parser);
static ASTNode *parse_expr_stmt(Parser *parser);

static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_equality(Parser *parser);
static ASTNode *parse_comparison(Parser *parser);
static ASTNode *parse_term(Parser *parser);
static ASTNode *parse_factor(Parser *parser);
static ASTNode *parse_call(Parser *parser);
static ASTNode *finish_call(Parser *parser, ASTNode *callee);
static ASTNode *parse_primary(Parser *parser);

ASTProgram *parse_program(Lexer *lexer) {
    Parser parser;
    parser_init(&parser, lexer);

    ASTProgram *program = ast_program_create();
    bool accepting_imports = true;

    parser_skip_newlines(&parser);
    while (!parser_check(&parser, TOKEN_EOF)) {
        if (parser_check(&parser, TOKEN_IMPORT)) {
            if (!accepting_imports) {
                parser_error(parser.current, "imports must appear before declarations");
            }
            ASTImport *import_stmt = parse_import(&parser);
            ast_program_add_import(program, import_stmt);
        } else {
            accepting_imports = false;
            ASTNode *decl = parse_top_level_decl(&parser);
            ast_program_add_declaration(program, decl);
        }
        parser_skip_newlines(&parser);
    }

    return program;
}

static void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->previous.type = TOKEN_EOF;
    parser->previous.lexeme = "";
    parser->previous.length = 0;
    parser->previous.line = 0;
    parser->previous.column = 0;
    parser->current = lexer_next_token(lexer);
    parser->next = lexer_next_token(lexer);
}

static void parser_advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = parser->next;
    parser->next = lexer_next_token(parser->lexer);
}

static bool parser_check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool parser_match(Parser *parser, TokenType type) {
    if (!parser_check(parser, type)) {
        return false;
    }
    parser_advance(parser);
    return true;
}

static Token parser_consume(Parser *parser, TokenType type, const char *message) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return parser->previous;
    }
    parser_error(parser->current, message);
    return parser->current;
}

static void parser_error(Token token, const char *message) {
    fprintf(stderr, "[line %d:%d] Parse error: %s\n", token.line, token.column, message);
    exit(EXIT_FAILURE);
}

static void parser_skip_newlines(Parser *parser) {
    while (parser_match(parser, TOKEN_NEWLINE)) {
        /* skip */
    }
}

static void parser_require_line_break(Parser *parser, const char *message) {
    if (parser_match(parser, TOKEN_NEWLINE)) {
        parser_skip_newlines(parser);
        return;
    }
    if (parser_check(parser, TOKEN_DEDENT) || parser_check(parser, TOKEN_EOF)) {
        return;
    }
    parser_error(parser->current, message);
}

static TokenType parser_peek_next(Parser *parser) {
    return parser->next.type;
}

static void type_builder_init(TypeBuilder *builder) {
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static void type_builder_append(TypeBuilder *builder, const char *text, size_t length) {
    if (length == 0) return;
    size_t needed = builder->length + length + 1;
    if (needed > builder->capacity) {
        size_t new_capacity = builder->capacity ? builder->capacity * 2 : 32;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        char *new_data = realloc(builder->data, new_capacity);
        if (!new_data) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        builder->data = new_data;
        builder->capacity = new_capacity;
    }
    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
}

static char *type_builder_finish(TypeBuilder *builder) {
    if (builder->capacity == 0) {
        builder->data = malloc(1);
        if (!builder->data) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }
        builder->capacity = 1;
    }
    builder->data[builder->length] = '\0';
    return builder->data;
}

static bool token_is_terminator(TokenType type, const TokenType *terminators, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (type == terminators[i]) {
            return true;
        }
    }
    return false;
}

static char *parser_collect_type(Parser *parser,
                                 const TokenType *terminators,
                                 size_t terminator_count) {
    TypeBuilder builder;
    type_builder_init(&builder);
    int bracket_depth = 0;
    bool collected = false;

    while (true) {
        TokenType type = parser->current.type;
        if (type == TOKEN_EOF) {
            break;
        }

        bool is_terminator = token_is_terminator(type, terminators, terminator_count);
        if ((type == TOKEN_NEWLINE || type == TOKEN_DEDENT) && bracket_depth == 0) {
            if (!is_terminator) {
                parser_error(parser->current, "unexpected line break in type");
            }
            break;
        }

        if (is_terminator && bracket_depth == 0) {
            break;
        }

        if (type == TOKEN_LBRACKET) {
            bracket_depth++;
        } else if (type == TOKEN_RBRACKET) {
            if (bracket_depth == 0) {
                parser_error(parser->current, "unmatched ']' in type");
            }
            bracket_depth--;
        }

        if (!(type == TOKEN_IDENT ||
              type == TOKEN_NULL ||
              type == TOKEN_COMMA ||
              type == TOKEN_LBRACKET ||
              type == TOKEN_RBRACKET ||
              type == TOKEN_DOT)) {
            if (bracket_depth == 0) {
                parser_error(parser->current, "unexpected token in type");
            }
        }

        type_builder_append(&builder, parser->current.lexeme, parser->current.length);
        collected = true;
        parser_advance(parser);
    }

    if (!collected) {
        parser_error(parser->current, "expected type name");
    }

    return type_builder_finish(&builder);
}

static ASTImport *parse_import(Parser *parser) {
    Token import_token = parser_consume(parser, TOKEN_IMPORT, "expected 'import'");
    ASTImport *import_stmt = ast_import_create(&import_token);

    bool expect_segment = true;
    while (expect_segment) {
        Token segment = parser_consume(parser, TOKEN_IDENT, "expected identifier in import path");
        ast_import_add_segment(import_stmt, &segment);
        if (!parser_match(parser, TOKEN_DOT)) {
            expect_segment = false;
        }
    }

    parser_require_line_break(parser, "expected newline after import statement");
    return import_stmt;
}

static ASTNode *parse_top_level_decl(Parser *parser) {
    bool is_public = parser_match(parser, TOKEN_PUB);

    if (parser_check(parser, TOKEN_STRUCT)) {
        return (ASTNode *)parse_struct_decl(parser, is_public);
    }

    Token name_token = parser_consume(parser, TOKEN_IDENT, "expected identifier for declaration");
    return (ASTNode *)parse_function_decl(parser, is_public, name_token);
}

static void free_string_array(ASTArray *array) {
    for (size_t i = 0; i < array->count; i++) {
        free(array->items[i]);
        array->items[i] = NULL;
    }
    ast_array_free(array);
}

static ASTFunctionDecl *parse_function_decl(Parser *parser, bool is_public, Token name_token) {
    ASTFunctionDecl *fn = ast_function_decl_create(is_public, &name_token);
    parse_function_params(parser, fn);
    ASTBlock *body = parse_block(parser, &name_token);
    ast_function_decl_set_body(fn, body);
    return fn;
}

static void parse_function_params(Parser *parser, ASTFunctionDecl *fn) {
    const TokenType type_terminators[] = { TOKEN_COMMA, TOKEN_RPAREN };
    ASTArray type_strings;
    ast_array_init(&type_strings);

    parser_consume(parser, TOKEN_COLON, "expected ':' after function name");
    parser_consume(parser, TOKEN_LPAREN, "expected '(' before parameter type list");

    if (!parser_check(parser, TOKEN_RPAREN)) {
        while (true) {
            char *type_name = parser_collect_type(parser, type_terminators, 2);
            ast_array_append(&type_strings, type_name);
            if (!parser_match(parser, TOKEN_COMMA)) {
                break;
            }
        }
    }
    parser_consume(parser, TOKEN_RPAREN, "expected ')' after parameter types");

    parser_consume(parser, TOKEN_ARROW, "expected '->' before return type");
    const TokenType return_terms[] = { TOKEN_EQUAL };
    char *return_type = parser_collect_type(parser, return_terms, 1);
    ast_function_decl_set_return_type(fn, return_type, strlen(return_type));
    free(return_type);

    parser_consume(parser, TOKEN_EQUAL, "expected '=' before parameter names");
    parser_consume(parser, TOKEN_LPAREN, "expected '(' before parameter names");

    size_t index = 0;
    if (!parser_check(parser, TOKEN_RPAREN)) {
        while (true) {
            Token name_token = parser_consume(parser, TOKEN_IDENT, "expected parameter name");
            if (index >= type_strings.count) {
                parser_error(name_token, "missing parameter type");
            }
            char *type_name = type_strings.items[index];
            ast_function_decl_add_param(fn, type_name, strlen(type_name), &name_token);
            free(type_name);
            type_strings.items[index] = NULL;
            index++;
            if (!parser_match(parser, TOKEN_COMMA)) {
                break;
            }
        }
    }
    parser_consume(parser, TOKEN_RPAREN, "expected ')' after parameter names");

    if (index != type_strings.count) {
        parser_error(parser->current, "mismatched parameter types and names");
    }

    free_string_array(&type_strings);
}

static ASTStructDecl *parse_struct_decl(Parser *parser, bool is_public) {
    Token struct_token = parser_consume(parser, TOKEN_STRUCT, "expected 'struct'");
    Token name_token = parser_consume(parser, TOKEN_IDENT, "expected struct name");
    ASTStructDecl *decl = ast_struct_decl_create(is_public, &name_token);

    parser_consume(parser, TOKEN_NEWLINE, "expected newline after struct name");
    parser_consume(parser, TOKEN_INDENT, "expected indent before struct body");
    parser_skip_newlines(parser);

    const TokenType field_terms[] = { TOKEN_NEWLINE, TOKEN_DEDENT };
    while (!parser_check(parser, TOKEN_DEDENT) && !parser_check(parser, TOKEN_EOF)) {
        Token field_name = parser_consume(parser, TOKEN_IDENT, "expected field name");
        parser_consume(parser, TOKEN_COLON, "expected ':' after field name");
        char *type_name = parser_collect_type(parser, field_terms, 2);
        ast_struct_decl_add_field(decl, &field_name, type_name, strlen(type_name));
        free(type_name);
        parser_require_line_break(parser, "expected newline after struct field");
        parser_skip_newlines(parser);
    }

    parser_consume(parser, TOKEN_DEDENT, "expected dedent after struct body");
    (void)struct_token;
    return decl;
}

static ASTBlock *parse_block(Parser *parser, const Token *start_token) {
    parser_consume(parser, TOKEN_NEWLINE, "expected newline before block");
    parser_consume(parser, TOKEN_INDENT, "expected indentation to start block");

    ASTBlock *block = ast_block_create(start_token);
    parser_skip_newlines(parser);

    while (!parser_check(parser, TOKEN_DEDENT) && !parser_check(parser, TOKEN_EOF)) {
        ASTNode *stmt = parse_statement(parser);
        ast_block_add_statement(block, stmt);
        parser_skip_newlines(parser);
    }

    parser_consume(parser, TOKEN_DEDENT, "expected dedent to close block");
    return block;
}

static ASTNode *parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_stmt(parser);
    }
    if (parser_match(parser, TOKEN_FOR)) {
        return parse_for_stmt(parser);
    }
    if (parser_match(parser, TOKEN_MUT)) {
        return parse_var_decl(parser, true);
    }
    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return(parser);
    }
    if (parser_check(parser, TOKEN_IDENT)) {
        TokenType lookahead = parser_peek_next(parser);
        if (lookahead == TOKEN_COLON) {
            return parse_var_decl(parser, false);
        }
        if (lookahead == TOKEN_EQUAL) {
            return parse_assignment(parser);
        }
    }
    return parse_expr_stmt(parser);
}

static ASTNode *parse_if_stmt(Parser *parser) {
    Token if_token = parser->previous;
    ASTNode *condition = parse_expression(parser);
    ASTBlock *then_block = parse_block(parser, &if_token);

    parser_skip_newlines(parser);
    ASTBlock *else_block = NULL;
    if (parser_match(parser, TOKEN_ELSE)) {
        else_block = parse_block(parser, &parser->previous);
    }

    return (ASTNode *)ast_if_create(&if_token, condition, then_block, else_block);
}

static ASTNode *parse_for_stmt(Parser *parser) {
    Token for_token = parser->previous;
    Token iterator = parser_consume(parser, TOKEN_IDENT, "expected loop iterator name");
    parser_consume(parser, TOKEN_IN, "expected 'in' after loop iterator");
    ASTNode *iterable = parse_expression(parser);
    ASTBlock *body = parse_block(parser, &for_token);

    return (ASTNode *)ast_for_create(&for_token, &iterator, iterable, body);
}

static ASTNode *parse_var_decl(Parser *parser, bool is_mutable) {
    Token name_token = parser_consume(parser, TOKEN_IDENT, is_mutable
        ? "expected identifier after 'mut'"
        : "expected identifier in variable declaration");
    parser_consume(parser, TOKEN_COLON, "expected ':' in variable declaration");

    const TokenType var_terms[] = { TOKEN_EQUAL };
    char *type_name = parser_collect_type(parser, var_terms, 1);
    parser_consume(parser, TOKEN_EQUAL, "expected '=' before initializer");
    ASTNode *initializer = parse_expression(parser);
    parser_require_line_break(parser, "expected newline after variable declaration");

    ASTVarDecl *decl = ast_var_decl_create(&name_token, is_mutable);
    ast_var_decl_set_type(decl, type_name, strlen(type_name));
    free(type_name);
    ast_var_decl_set_initializer(decl, initializer);
    return (ASTNode *)decl;
}

static ASTNode *parse_assignment(Parser *parser) {
    Token name_token = parser_consume(parser, TOKEN_IDENT, "expected identifier for assignment");
    parser_consume(parser, TOKEN_EQUAL, "expected '=' in assignment");
    ASTNode *value = parse_expression(parser);
    parser_require_line_break(parser, "expected newline after assignment");
    return (ASTNode *)ast_assign_create(&name_token, value);
}

static ASTNode *parse_return(Parser *parser) {
    Token return_token = parser->previous;
    ASTNode *value = NULL;
    if (!parser_check(parser, TOKEN_NEWLINE) &&
        !parser_check(parser, TOKEN_DEDENT) &&
        !parser_check(parser, TOKEN_EOF)) {
        value = parse_expression(parser);
    }
    parser_require_line_break(parser, "expected newline after return");
    return (ASTNode *)ast_return_create(&return_token, value);
}

static ASTNode *parse_expr_stmt(Parser *parser) {
    ASTNode *expr = parse_expression(parser);
    parser_require_line_break(parser, "expected newline after expression");
    return (ASTNode *)ast_expr_stmt_create(expr);
}

static ASTNode *parse_expression(Parser *parser) {
    return parse_equality(parser);
}

static ASTNode *parse_equality(Parser *parser) {
    ASTNode *expr = parse_comparison(parser);
    while (parser_check(parser, TOKEN_EQEQ) || parser_check(parser, TOKEN_BANGEQ)) {
        Token op_token = parser->current;
        parser_advance(parser);
        ASTNode *right = parse_comparison(parser);
        expr = (ASTNode *)ast_binary_create(expr, op_token.type, right, &op_token);
    }
    return expr;
}

static ASTNode *parse_comparison(Parser *parser) {
    ASTNode *expr = parse_term(parser);
    while (parser_check(parser, TOKEN_LT) || parser_check(parser, TOKEN_LTE) ||
           parser_check(parser, TOKEN_GT) || parser_check(parser, TOKEN_GTE)) {
        Token op_token = parser->current;
        parser_advance(parser);
        ASTNode *right = parse_term(parser);
        expr = (ASTNode *)ast_binary_create(expr, op_token.type, right, &op_token);
    }
    return expr;
}

static ASTNode *parse_term(Parser *parser) {
    ASTNode *expr = parse_factor(parser);
    while (parser_check(parser, TOKEN_PLUS) || parser_check(parser, TOKEN_MINUS)) {
        Token op_token = parser->current;
        parser_advance(parser);
        ASTNode *right = parse_factor(parser);
        expr = (ASTNode *)ast_binary_create(expr, op_token.type, right, &op_token);
    }
    return expr;
}

static ASTNode *parse_factor(Parser *parser) {
    ASTNode *expr = parse_call(parser);
    while (parser_check(parser, TOKEN_STAR) || parser_check(parser, TOKEN_SLASH)) {
        Token op_token = parser->current;
        parser_advance(parser);
        ASTNode *right = parse_call(parser);
        expr = (ASTNode *)ast_binary_create(expr, op_token.type, right, &op_token);
    }
    return expr;
}

static ASTNode *parse_call(Parser *parser) {
    ASTNode *expr = parse_primary(parser);

    while (parser_match(parser, TOKEN_LPAREN)) {
        expr = finish_call(parser, expr);
    }

    return expr;
}

static ASTNode *finish_call(Parser *parser, ASTNode *callee) {
    Token lparen = parser->previous;
    ASTCallExpr *call = ast_call_create(callee, &lparen);

    if (!parser_check(parser, TOKEN_RPAREN)) {
        while (true) {
            ASTNode *argument = parse_expression(parser);
            ast_call_add_argument(call, argument);
            if (!parser_match(parser, TOKEN_COMMA)) {
                break;
            }
        }
    }

    parser_consume(parser, TOKEN_RPAREN, "expected ')' after arguments");
    return (ASTNode *)call;
}

static ASTNode *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_INT)) {
        ASTLiteralExpr *expr = ast_literal_create(&parser->previous, AST_LITERAL_INT);
        ast_literal_set_text(expr, parser->previous.lexeme, parser->previous.length);
        return (ASTNode *)expr;
    }
    if (parser_match(parser, TOKEN_FLOAT)) {
        ASTLiteralExpr *expr = ast_literal_create(&parser->previous, AST_LITERAL_FLOAT);
        ast_literal_set_text(expr, parser->previous.lexeme, parser->previous.length);
        return (ASTNode *)expr;
    }
    if (parser_match(parser, TOKEN_STRING)) {
        ASTLiteralExpr *expr = ast_literal_create(&parser->previous, AST_LITERAL_STRING);
        ast_literal_set_text(expr, parser->previous.lexeme, parser->previous.length);
        return (ASTNode *)expr;
    }
    if (parser_match(parser, TOKEN_TRUE)) {
        ASTLiteralExpr *expr = ast_literal_create(&parser->previous, AST_LITERAL_BOOL);
        ast_literal_set_bool(expr, true);
        return (ASTNode *)expr;
    }
    if (parser_match(parser, TOKEN_FALSE)) {
        ASTLiteralExpr *expr = ast_literal_create(&parser->previous, AST_LITERAL_BOOL);
        ast_literal_set_bool(expr, false);
        return (ASTNode *)expr;
    }
    if (parser_match(parser, TOKEN_NULL)) {
        return (ASTNode *)ast_literal_create(&parser->previous, AST_LITERAL_NULL);
    }
    if (parser_match(parser, TOKEN_IDENT)) {
        return (ASTNode *)ast_identifier_create(&parser->previous);
    }
    if (parser_match(parser, TOKEN_LPAREN)) {
        ASTNode *expr = parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, "expected ')' after expression");
        return expr;
    }

    parser_error(parser->current, "unexpected token in expression");
    return NULL;
}
