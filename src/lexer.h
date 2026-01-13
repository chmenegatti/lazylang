#ifndef LZ_LEXER_H
#define LZ_LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    TOKEN_IDENT,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,

    // Keywords
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_STRUCT,
    TOKEN_MUT,
    TOKEN_PUB,
    TOKEN_IMPORT,
    TOKEN_TASK,
    TOKEN_RETURN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    // Symbols
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_ARROW,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_DOT,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQEQ,
    TOKEN_BANGEQ,
    TOKEN_LT,
    TOKEN_LTE,
    TOKEN_GT,
    TOKEN_GTE,

} TokenType;

typedef struct {
    TokenType type;
    const char *lexeme;
    size_t length;
    int line;
    int column;
} Token;

typedef struct Lexer Lexer;

Lexer *lexer_create(const char *source);

void lexer_destroy(Lexer *lexer);

Token lexer_next_token(Lexer *lexer);

const char *token_type_name(TokenType type);

#endif
