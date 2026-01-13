#include "lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <source-file>\n", argv[0]);
        return 1;
    }

    char *source = read_file(argv[1]);
    Lexer *lexer = lexer_create(source);
    ASTProgram *program = parse_program(lexer);

        printf("Parsed %zu import(s) and %zu declaration(s)\n",
            program->imports.count,
            program->declarations.count);

        sema_check_program(program);
        printf("Semantic analysis completed successfully\n");

    ast_program_destroy(program);
    lexer_destroy(lexer);
    free(source);
    return 0;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "failed to open '%s'\n", path);
        exit(EXIT_FAILURE);
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek '%s'\n", path);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    long size = ftell(file);
    if (size < 0) {
        fprintf(stderr, "failed to measure '%s'\n", path);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "failed to rewind '%s'\n", path);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fprintf(stderr, "Out of memory\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    if (read != (size_t)size) {
        fprintf(stderr, "failed to read '%s'\n", path);
        free(buffer);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    buffer[size] = '\0';
    fclose(file);
    return buffer;
}
