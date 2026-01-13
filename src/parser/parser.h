#ifndef LZ_PARSER_H
#define LZ_PARSER_H

#include "../lexer.h"
#include "../ast/ast.h"

ASTProgram *parse_program(Lexer *lexer);

#endif
