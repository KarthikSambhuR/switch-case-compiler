#include "switchc.h"
#include "driver.h"
#include "json_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define IS_TERMINAL(stream) _isatty(_fileno(stream))
#else
#include <unistd.h>
#define IS_TERMINAL(stream) isatty(fileno(stream))
#endif

int yyparse(void);
void yy_scan_string(const char *text);
void yylex_destroy(void);

DiagnosticList *g_diagnostics = NULL;
Program *g_program = NULL;
int g_show_tokens = 0;

static void print_usage(const char *program)
{
    printf("Switch Statement Compiler - Flex/Bison Front-End\n\n");
    printf("Usage: %s [options] [file.c]\n\n", program);
    printf("Options:\n");
    printf("  -e, --code TEXT    compile TEXT instead of a file or stdin\n");
    printf("      --query TEXT   alias for --code\n");
    printf("      --tokens       print the token stream produced by Flex\n");
    printf("      --ast          print the parse tree / AST (default)\n");
    printf("      --no-ast       suppress the AST\n");
    printf("      --json         emit machine-readable analysis JSON\n");
    printf("      --quiet        print only diagnostics and the final result\n");
    printf("  -h, --help         show this help text\n\n");
    printf("Exit codes: 0 valid, 1 lexical/syntax error, 2 semantic error, 3 usage/I/O error\n");
}

static char *read_stream(FILE *stream)
{
    size_t capacity = 1024;
    size_t length = 0;
    char *buffer = (char *)allocate_memory(capacity);

    for (;;) {
        size_t available = capacity - length - 1;
        size_t read_count;
        if (available == 0) {
            capacity *= 2;
            buffer = (char *)resize_memory(buffer, capacity);
            available = capacity - length - 1;
        }
        read_count = fread(buffer + length, 1, available, stream);
        length += read_count;
        if (read_count < available) {
            break;
        }
    }
    buffer[length] = '\0';
    return buffer;
}

static char *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    char *content;
    if (file == NULL) {
        fprintf(stderr, "error: cannot open file '%s'\n", path);
        return NULL;
    }
    content = read_stream(file);
    fclose(file);
    return content;
}

static void strip_utf8_bom(char *text)
{
    unsigned char *bytes = (unsigned char *)text;
    size_t length = strlen(text);
    if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        memmove(text, text + 3, length - 2);
    }
}

static int count_code_prefix(const DiagnosticList *diagnostics, char prefix)
{
    int count = 0;
    int index;
    for (index = 0; index < diagnostics->count; index++) {
        if (diagnostics->items[index].code[0] == prefix) {
            count++;
        }
    }
    return count;
}

static void print_indent(int level)
{
    int i;
    for (i = 0; i < level; i++) {
        printf("  ");
    }
}

static void print_ast_expr(const Expression *expr, int indent);

static void print_ast_stmt(const Statement *stmt, int indent)
{
    int i;
    if (stmt == NULL) return;
    print_indent(indent);
    switch (stmt->kind) {
        case STMT_SWITCH:
            printf("SwitchStmt [%d:%d]\n", stmt->line, stmt->column);
            print_indent(indent + 1);
            printf("Condition:\n");
            print_ast_expr(stmt->as.switch_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            print_ast_stmt(stmt->as.switch_stmt.body, indent + 2);
            break;
        case STMT_CASE:
            printf("CaseLabel [%d:%d] (const_val = %lld, is_duplicate = %s)\n",
                   stmt->line, stmt->column,
                   (long long)stmt->as.case_stmt.const_val,
                   stmt->as.case_stmt.is_duplicate ? "true" : "false");
            print_indent(indent + 1);
            printf("Expression:\n");
            print_ast_expr(stmt->as.case_stmt.expr, indent + 2);
            if (stmt->as.case_stmt.statements.count > 0) {
                print_indent(indent + 1);
                printf("Statements (%d):\n", stmt->as.case_stmt.statements.count);
                for (i = 0; i < stmt->as.case_stmt.statements.count; i++) {
                    print_ast_stmt(stmt->as.case_stmt.statements.items[i], indent + 2);
                }
            }
            break;
        case STMT_DEFAULT:
            printf("DefaultLabel [%d:%d]\n", stmt->line, stmt->column);
            if (stmt->as.default_stmt.statements.count > 0) {
                print_indent(indent + 1);
                printf("Statements (%d):\n", stmt->as.default_stmt.statements.count);
                for (i = 0; i < stmt->as.default_stmt.statements.count; i++) {
                    print_ast_stmt(stmt->as.default_stmt.statements.items[i], indent + 2);
                }
            }
            break;
        case STMT_BREAK:
            printf("BreakStmt [%d:%d]\n", stmt->line, stmt->column);
            break;
        case STMT_BLOCK:
            printf("BlockStmt [%d:%d] (%d items)\n", stmt->line, stmt->column,
                   stmt->as.block.statements.count);
            for (i = 0; i < stmt->as.block.statements.count; i++) {
                print_ast_stmt(stmt->as.block.statements.items[i], indent + 1);
            }
            break;
        case STMT_ASSIGN:
            printf("AssignStmt [%d:%d] %s =\n", stmt->line, stmt->column, stmt->as.assign.name);
            print_ast_expr(stmt->as.assign.value, indent + 1);
            break;
        case STMT_VAR_DECL:
            printf("VarDeclStmt [%d:%d] %s %s\n", stmt->line, stmt->column,
                   stmt->as.var_decl.type_name, stmt->as.var_decl.name);
            if (stmt->as.var_decl.init != NULL) {
                print_indent(indent + 1);
                printf("Init:\n");
                print_ast_expr(stmt->as.var_decl.init, indent + 2);
            }
            break;
        case STMT_EXPR:
            printf("ExprStmt [%d:%d]\n", stmt->line, stmt->column);
            print_ast_expr(stmt->as.expr_stmt.expr, indent + 1);
            break;
        case STMT_EMPTY:
            printf("EmptyStmt [%d:%d]\n", stmt->line, stmt->column);
            break;
    }
}

static void print_ast_expr(const Expression *expr, int indent)
{
    if (expr == NULL) return;
    print_indent(indent);
    switch (expr->kind) {
        case EXPR_INTEGER_LITERAL:
            printf("IntegerLiteral: %lld (raw: %s) [%d:%d]\n",
                   (long long)expr->as.integer_lit.value,
                   expr->as.integer_lit.raw_text ? expr->as.integer_lit.raw_text : "",
                   expr->line, expr->column);
            break;
        case EXPR_FLOAT_LITERAL:
            printf("FloatLiteral: %g [%d:%d]\n", expr->as.float_lit.value, expr->line, expr->column);
            break;
        case EXPR_CHAR_LITERAL:
            printf("CharLiteral: '%c' (ascii %d) [%d:%d]\n",
                   expr->as.char_lit.value, (int)(unsigned char)expr->as.char_lit.value,
                   expr->line, expr->column);
            break;
        case EXPR_STRING_LITERAL:
            printf("StringLiteral: \"%s\" [%d:%d]\n",
                   expr->as.string_lit.text ? expr->as.string_lit.text : "",
                   expr->line, expr->column);
            break;
        case EXPR_IDENTIFIER:
            printf("Identifier: %s [%d:%d]\n",
                   expr->as.identifier.name ? expr->as.identifier.name : "",
                   expr->line, expr->column);
            break;
        case EXPR_UNARY:
            printf("UnaryExpr [%d:%d]\n", expr->line, expr->column);
            print_ast_expr(expr->as.unary.operand, indent + 1);
            break;
        case EXPR_BINARY:
            printf("BinaryExpr [%d:%d]\n", expr->line, expr->column);
            print_ast_expr(expr->as.binary.left, indent + 1);
            print_ast_expr(expr->as.binary.right, indent + 1);
            break;
        case EXPR_PAREN:
            printf("ParenExpr [%d:%d]\n", expr->line, expr->column);
            print_ast_expr(expr->as.paren.inner, indent + 1);
            break;
    }
}

static void print_human_ast(const Program *program)
{
    int i;
    if (program == NULL) return;
    printf("=== Abstract Syntax Tree ===\n");
    for (i = 0; i < program->statements.count; i++) {
        print_ast_stmt(program->statements.items[i], 0);
    }
}

int main(int argc, char **argv)
{
    const char *code_argument = NULL;
    const char *file_path = NULL;
    int show_tokens = 0;
    int show_ast = 1;
    int quiet = 0;
    int json_mode = 0;
    int index;
    char *source_text = NULL;
    DiagnosticList diagnostics;
    int parse_status;
    int syntax_ok;
    int lex_errors;
    int parse_errors;
    int semantic_errors;
    int exit_code = 0;

    for (index = 1; index < argc; index++) {
        const char *arg = argv[index];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-e") == 0 || strcmp(arg, "--code") == 0 || strcmp(arg, "--query") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "error: %s requires an argument\n", arg);
                return 3;
            }
            code_argument = argv[++index];
            continue;
        }
        if (strcmp(arg, "--tokens") == 0) {
            show_tokens = 1;
            continue;
        }
        if (strcmp(arg, "--ast") == 0) {
            show_ast = 1;
            continue;
        }
        if (strcmp(arg, "--no-ast") == 0) {
            show_ast = 0;
            continue;
        }
        if (strcmp(arg, "--json") == 0) {
            json_mode = 1;
            continue;
        }
        if (strcmp(arg, "--quiet") == 0) {
            quiet = 1;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "error: unrecognized option '%s'\n", arg);
            return 3;
        }
        if (file_path != NULL) {
            fprintf(stderr, "error: only one input file may be specified\n");
            return 3;
        }
        file_path = arg;
    }

    if (code_argument != NULL) {
        source_text = duplicate_string(code_argument);
    } else if (file_path != NULL) {
        source_text = read_file(file_path);
        if (source_text == NULL) {
            return 3;
        }
    } else {
        source_text = read_stream(stdin);
    }

    strip_utf8_bom(source_text);

    diag_init(&diagnostics);
    g_diagnostics = &diagnostics;
    g_show_tokens = show_tokens && !json_mode;
    g_json_mode = json_mode;
    if (json_mode) {
        json_tokens_init();
    }

    yy_scan_string(source_text);
    parse_status = yyparse();
    yylex_destroy();

    lex_errors = count_code_prefix(&diagnostics, 'L');
    parse_errors = count_code_prefix(&diagnostics, 'P');
    syntax_ok = (parse_status == 0 && lex_errors == 0 && parse_errors == 0);

    if (syntax_ok) {
        semantic_validate_program(g_program, &diagnostics);
    }

    semantic_errors = count_code_prefix(&diagnostics, 'S');

    if (!syntax_ok) {
        exit_code = 1;
    } else if (semantic_errors > 0) {
        exit_code = 2;
    } else {
        exit_code = 0;
    }

    if (json_mode) {
        json_print_result(g_program, &diagnostics, syntax_ok, lex_errors);
        json_tokens_free();
    } else {
        if (!quiet && show_ast && syntax_ok) {
            print_human_ast(g_program);
        }
        if (diagnostics.count > 0) {
            diag_print(&diagnostics);
        }
        if (!quiet) {
            if (exit_code == 0) {
                printf("\nStatus: VALID - switch statement validated with 0 errors\n");
            } else if (exit_code == 1) {
                printf("\nStatus: FAILED - syntax/lexical error encountered\n");
            } else {
                printf("\nStatus: FAILED - %d semantic error(s) detected\n", semantic_errors);
            }
        }
    }

    program_free(g_program);
    diag_free(&diagnostics);
    free(source_text);

    return exit_code;
}
