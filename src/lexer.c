#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define INDENT_STACK_MAX 128

struct Lexer {
    const char *src;
    size_t pos;
    int line;
    int col;

    int indent_stack[INDENT_STACK_MAX];
    int indent_top;

    int pending_dedents;
    bool at_line_start;
};

/* ---------- utilidades básicas ---------- */

static char peek(Lexer *l) {
    return l->src[l->pos];
}

static char advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static bool match(Lexer *l, char expected) {
    if (peek(l) != expected) return false;
    advance(l);
    return true;
}

static Token make_token(Lexer *l, TokenType type, const char *start, size_t len) {
    Token t;
    t.type = type;
    t.lexeme = start;
    t.length = len;
    t.line = l->line;
    t.column = l->col;
    return t;
}

/* ---------- criação / destruição ---------- */

Lexer *lexer_create(const char *source) {
    Lexer *l = malloc(sizeof(Lexer));
    l->src = source;
    l->pos = 0;
    l->line = 1;
    l->col = 1;

    l->indent_stack[0] = 0;
    l->indent_top = 0;
    l->pending_dedents = 0;
    l->at_line_start = true;

    return l;
}

void lexer_destroy(Lexer *l) {
    free(l);
}

/* ---------- indentação ---------- */

static int count_indent(Lexer *l) {
    int count = 0;
    while (peek(l) == ' ' || peek(l) == '\t') {
        advance(l);
        count++;
    }
    return count;
}

/* ---------- identificadores / keywords ---------- */

static Token identifier(Lexer *l) {
    size_t start = l->pos - 1;
    while (isalnum(peek(l)) || peek(l) == '_') {
        advance(l);
    }

    size_t len = l->pos - start;
    const char *text = l->src + start;

#define KW(name, tok) \
    if (len == strlen(name) && strncmp(text, name, len) == 0) \
        return make_token(l, tok, text, len);

    KW("if", TOKEN_IF)
    KW("else", TOKEN_ELSE)
    KW("for", TOKEN_FOR)
    KW("in", TOKEN_IN)
    KW("struct", TOKEN_STRUCT)
    KW("mut", TOKEN_MUT)
    KW("pub", TOKEN_PUB)
    KW("import", TOKEN_IMPORT)
    KW("task", TOKEN_TASK)
    KW("return", TOKEN_RETURN)
    KW("true", TOKEN_TRUE)
    KW("false", TOKEN_FALSE)
    KW("null", TOKEN_NULL)

#undef KW

    return make_token(l, TOKEN_IDENT, text, len);
}

/* ---------- lexer principal ---------- */

Token lexer_next_token(Lexer *l) {

    /* DEDENTs pendentes */
    if (l->pending_dedents > 0) {
        l->pending_dedents--;
        return make_token(l, TOKEN_DEDENT, "", 0);
    }

    /* início de linha → analisar indentação */
    if (l->at_line_start) {
        l->at_line_start = false;

        int indent = count_indent(l);
        int current = l->indent_stack[l->indent_top];

        if (indent > current) {
            l->indent_stack[++l->indent_top] = indent;
            return make_token(l, TOKEN_INDENT, "", 0);
        }

        if (indent < current) {
            while (l->indent_top > 0 &&
                   indent < l->indent_stack[l->indent_top]) {
                l->indent_top--;
                l->pending_dedents++;
            }

            if (indent != l->indent_stack[l->indent_top]) {
                fprintf(stderr,
                        "Indentation error at line %d\n",
                        l->line);
                exit(1);
            }

            l->pending_dedents--;
            return make_token(l, TOKEN_DEDENT, "", 0);
        }
    }

    /* consumo normal de caracteres */
    for (;;) {
        char c = advance(l);

        if (c == '\0') {
            if (l->indent_top > 0) {
                l->indent_top--;
                return make_token(l, TOKEN_DEDENT, "", 0);
            }
            return make_token(l, TOKEN_EOF, "", 0);
        }

        if (c == ' ' || c == '\t' || c == '\r') {
            continue;
        }

        if (c == '\n') {
            l->at_line_start = true;
            return make_token(l, TOKEN_NEWLINE, "\n", 1);
        }

        if (isalpha(c) || c == '_') {
            return identifier(l);
        }

        if (isdigit(c)) {
            size_t start = l->pos - 1;
            while (isdigit(peek(l))) advance(l);

            if (peek(l) == '.') {
                advance(l);
                while (isdigit(peek(l))) advance(l);
                return make_token(l, TOKEN_FLOAT,
                                  l->src + start,
                                  l->pos - start);
            }

            return make_token(l, TOKEN_INT,
                              l->src + start,
                              l->pos - start);
        }

        if (c == '"') {
            size_t start = l->pos;
            while (peek(l) != '"' && peek(l) != '\0') {
                advance(l);
            }
            if (peek(l) == '"') advance(l);
            return make_token(l, TOKEN_STRING,
                              l->src + start,
                              l->pos - start - 1);
        }

        switch (c) {
            case ':': return make_token(l, TOKEN_COLON, ":", 1);
            case ',': return make_token(l, TOKEN_COMMA, ",", 1);
            case '=':
                if (match(l, '=')) {
                    return make_token(l, TOKEN_EQEQ, "==", 2);
                }
                return make_token(l, TOKEN_EQUAL, "=", 1);
            case '-':
                if (match(l, '>')) {
                    return make_token(l, TOKEN_ARROW, "->", 2);
                }
                return make_token(l, TOKEN_MINUS, "-", 1);
            case '+':
                return make_token(l, TOKEN_PLUS, "+", 1);
            case '*':
                return make_token(l, TOKEN_STAR, "*", 1);
            case '/':
                return make_token(l, TOKEN_SLASH, "/", 1);
            case '!':
                if (match(l, '=')) {
                    return make_token(l, TOKEN_BANGEQ, "!=", 2);
                }
                fprintf(stderr,
                        "Unexpected '!' at line %d, column %d\n",
                        l->line,
                        l->col);
                exit(1);
            case '<':
                if (match(l, '=')) {
                    return make_token(l, TOKEN_LTE, "<=", 2);
                }
                return make_token(l, TOKEN_LT, "<", 1);
            case '>':
                if (match(l, '=')) {
                    return make_token(l, TOKEN_GTE, ">=", 2);
                }
                return make_token(l, TOKEN_GT, ">", 1);
            case '(':
                return make_token(l, TOKEN_LPAREN, "(", 1);
            case ')':
                return make_token(l, TOKEN_RPAREN, ")", 1);
            case '.':
                return make_token(l, TOKEN_DOT, ".", 1);
            case '[':
                return make_token(l, TOKEN_LBRACKET, "[", 1);
            case ']':
                return make_token(l, TOKEN_RBRACKET, "]", 1);
        }
    }
}

/* ---------- debug ---------- */

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:     return "EOF";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT:  return "INDENT";
        case TOKEN_DEDENT:  return "DEDENT";

        case TOKEN_IDENT:   return "IDENT";
        case TOKEN_INT:     return "INT";
        case TOKEN_FLOAT:   return "FLOAT";
        case TOKEN_STRING:  return "STRING";

        case TOKEN_IF:      return "IF";
        case TOKEN_ELSE:    return "ELSE";
        case TOKEN_FOR:     return "FOR";
        case TOKEN_IN:      return "IN";
        case TOKEN_STRUCT:  return "STRUCT";
        case TOKEN_MUT:     return "MUT";
        case TOKEN_PUB:     return "PUB";
        case TOKEN_IMPORT:  return "IMPORT";
        case TOKEN_TASK:    return "TASK";
        case TOKEN_RETURN:  return "RETURN";
        case TOKEN_TRUE:    return "TRUE";
        case TOKEN_FALSE:   return "FALSE";
        case TOKEN_NULL:    return "NULL";

        case TOKEN_COLON:   return "COLON";
        case TOKEN_COMMA:   return "COMMA";
        case TOKEN_EQUAL:   return "EQUAL";
        case TOKEN_EQEQ:    return "EQEQ";
        case TOKEN_BANGEQ:  return "BANGEQ";
        case TOKEN_ARROW:   return "ARROW";
        case TOKEN_LPAREN:  return "LPAREN";
        case TOKEN_RPAREN:  return "RPAREN";
        case TOKEN_DOT:     return "DOT";
        case TOKEN_LBRACKET:return "LBRACKET";
        case TOKEN_RBRACKET:return "RBRACKET";
        case TOKEN_PLUS:    return "PLUS";
        case TOKEN_MINUS:   return "MINUS";
        case TOKEN_STAR:    return "STAR";
        case TOKEN_SLASH:   return "SLASH";
        case TOKEN_LT:      return "LT";
        case TOKEN_LTE:     return "LTE";
        case TOKEN_GT:      return "GT";
        case TOKEN_GTE:     return "GTE";

        default:            return "UNKNOWN";
    }
}
