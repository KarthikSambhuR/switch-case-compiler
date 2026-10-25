#ifndef JSON_OUTPUT_H
#define JSON_OUTPUT_H

#include "switchc.h"

typedef struct {
    char *type;
    char *lexeme;
    int line;
    int column;
} JsonToken;

typedef struct {
    JsonToken *items;
    int count;
    int capacity;
} JsonTokenList;

extern int g_json_mode;
extern JsonTokenList g_json_tokens;

void json_tokens_init(void);
void json_tokens_add(const char *type, const char *lexeme, int line, int column);
void json_tokens_free(void);

void json_print_result(const Program *program, const DiagnosticList *diagnostics,
                       int syntax_ok, int lex_errors);

#endif
