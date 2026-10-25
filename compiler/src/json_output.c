#include "json_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_json_mode = 0;
JsonTokenList g_json_tokens = {NULL, 0, 0};

void json_tokens_init(void)
{
    g_json_tokens.items = NULL;
    g_json_tokens.count = 0;
    g_json_tokens.capacity = 0;
}

void json_tokens_add(const char *type, const char *lexeme, int line, int column)
{
    JsonToken *item;
    if (!g_json_mode) {
        return;
    }
    if (g_json_tokens.count == g_json_tokens.capacity) {
        int capacity = g_json_tokens.capacity == 0 ? 32 : g_json_tokens.capacity * 2;
        JsonToken *items = (JsonToken *)realloc(g_json_tokens.items, (size_t)capacity * sizeof(JsonToken));
        if (items == NULL) {
            fprintf(stderr, "fatal: out of memory\n");
            exit(3);
        }
        g_json_tokens.items = items;
        g_json_tokens.capacity = capacity;
    }
    item = &g_json_tokens.items[g_json_tokens.count++];
    item->type = duplicate_string(type != NULL ? type : "TOKEN");
    item->lexeme = duplicate_string(lexeme != NULL ? lexeme : "");
    item->line = line;
    item->column = column;
}

void json_tokens_free(void)
{
    int index;
    for (index = 0; index < g_json_tokens.count; index++) {
        free(g_json_tokens.items[index].type);
        free(g_json_tokens.items[index].lexeme);
    }
    free(g_json_tokens.items);
    g_json_tokens.items = NULL;
    g_json_tokens.count = 0;
    g_json_tokens.capacity = 0;
}

static void json_string(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)(value != NULL ? value : "");
    putchar('"');
    while (*cursor != '\0') {
        switch (*cursor) {
            case '"': fputs("\\\"", stdout); cursor++; break;
            case '\\': fputs("\\\\", stdout); cursor++; break;
            case '\b': fputs("\\b", stdout); cursor++; break;
            case '\f': fputs("\\f", stdout); cursor++; break;
            case '\n': fputs("\\n", stdout); cursor++; break;
            case '\r': fputs("\\r", stdout); cursor++; break;
            case '\t': fputs("\\t", stdout); cursor++; break;
            default:
                if (*cursor < 0x20) {
                    printf("\\u%04x", *cursor);
                    cursor++;
                } else {
                    putchar(*cursor);
                    cursor++;
                }
        }
    }
    putchar('"');
}

static const char *unary_op_name(UnaryOp op)
{
    switch (op) {
        case UNARY_PLUS: return "+";
        case UNARY_MINUS: return "-";
        case UNARY_BITWISE_NOT: return "~";
        case UNARY_LOGICAL_NOT: return "!";
        default: return "?";
    }
}

static const char *binary_op_name(BinaryOp op)
{
    switch (op) {
        case BIN_ADD: return "+";
        case BIN_SUB: return "-";
        case BIN_MUL: return "*";
        case BIN_DIV: return "/";
        case BIN_MOD: return "%";
        case BIN_EQ: return "==";
        case BIN_NE: return "!=";
        case BIN_LT: return "<";
        case BIN_LE: return "<=";
        case BIN_GT: return ">";
        case BIN_GE: return ">=";
        case BIN_BIT_AND: return "&";
        case BIN_BIT_OR: return "|";
        case BIN_BIT_XOR: return "^";
        case BIN_SHL: return "<<";
        case BIN_SHR: return ">>";
        case BIN_LOG_AND: return "&&";
        case BIN_LOG_OR: return "||";
        default: return "?";
    }
}

static void print_expression(const Expression *expr);
static void print_statement(const Statement *stmt);

static void print_expression(const Expression *expr)
{
    if (expr == NULL) {
        fputs("null", stdout);
        return;
    }
    switch (expr->kind) {
        case EXPR_INTEGER_LITERAL:
            printf("{\"kind\":\"integer\",\"value\":%lld,\"raw\":", (long long)expr->as.integer_lit.value);
            json_string(expr->as.integer_lit.raw_text);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_FLOAT_LITERAL:
            printf("{\"kind\":\"float\",\"value\":%.15g,\"raw\":", expr->as.float_lit.value);
            json_string(expr->as.float_lit.raw_text);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_CHAR_LITERAL: {
            char buf[8];
            snprintf(buf, sizeof(buf), "%c", expr->as.char_lit.value);
            printf("{\"kind\":\"char\",\"value\":%d,\"char\":", (int)(unsigned char)expr->as.char_lit.value);
            json_string(buf);
            fputs(",\"raw\":", stdout);
            json_string(expr->as.char_lit.raw_text);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        }
        case EXPR_STRING_LITERAL:
            fputs("{\"kind\":\"string\",\"value\":", stdout);
            json_string(expr->as.string_lit.text);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_IDENTIFIER:
            fputs("{\"kind\":\"identifier\",\"name\":", stdout);
            json_string(expr->as.identifier.name);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_UNARY:
            fputs("{\"kind\":\"unary\",\"op\":", stdout);
            json_string(unary_op_name(expr->as.unary.op));
            fputs(",\"operand\":", stdout);
            print_expression(expr->as.unary.operand);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_BINARY:
            fputs("{\"kind\":\"binary\",\"op\":", stdout);
            json_string(binary_op_name(expr->as.binary.op));
            fputs(",\"left\":", stdout);
            print_expression(expr->as.binary.left);
            fputs(",\"right\":", stdout);
            print_expression(expr->as.binary.right);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
        case EXPR_PAREN:
            fputs("{\"kind\":\"paren\",\"inner\":", stdout);
            print_expression(expr->as.paren.inner);
            printf(",\"line\":%d,\"column\":%d}", expr->line, expr->column);
            break;
    }
}

static void print_statement_list(const StatementList *list)
{
    int i;
    putchar('[');
    if (list != NULL) {
        for (i = 0; i < list->count; i++) {
            if (i > 0) putchar(',');
            print_statement(list->items[i]);
        }
    }
    putchar(']');
}

static void print_statement(const Statement *stmt)
{
    int i;
    if (stmt == NULL) {
        fputs("null", stdout);
        return;
    }
    switch (stmt->kind) {
        case STMT_SWITCH:
            printf("{\"kind\":\"switch\",\"line\":%d,\"column\":%d,\"hasDefault\":%s,\"defaultLine\":%d,\"defaultColumn\":%d,\"condition\":",
                   stmt->line, stmt->column,
                   stmt->as.switch_stmt.has_default ? "true" : "false",
                   stmt->as.switch_stmt.default_line, stmt->as.switch_stmt.default_column);
            print_expression(stmt->as.switch_stmt.condition);
            fputs(",\"caseEntries\":[", stdout);
            for (i = 0; i < stmt->as.switch_stmt.case_entries.count; i++) {
                CaseEntry *e = &stmt->as.switch_stmt.case_entries.items[i];
                if (i > 0) putchar(',');
                printf("{\"line\":%d,\"column\":%d,\"constValue\":%lld,\"isValidConst\":%s,\"isDuplicate\":%s,\"prevLine\":%d,\"prevColumn\":%d,\"exprText\":",
                       e->line, e->column, (long long)e->const_val,
                       e->is_valid_const ? "true" : "false",
                       e->is_duplicate ? "true" : "false",
                       e->prev_line, e->prev_column);
                json_string(e->expr_text);
                putchar('}');
            }
            fputs("],\"body\":", stdout);
            print_statement(stmt->as.switch_stmt.body);
            putchar('}');
            break;

        case STMT_CASE:
            printf("{\"kind\":\"case\",\"line\":%d,\"column\":%d,\"constValue\":%lld,\"isValidConst\":%s,\"isDuplicate\":%s,\"expr\":",
                   stmt->line, stmt->column, (long long)stmt->as.case_stmt.const_val,
                   stmt->as.case_stmt.is_valid_const ? "true" : "false",
                   stmt->as.case_stmt.is_duplicate ? "true" : "false");
            print_expression(stmt->as.case_stmt.expr);
            fputs(",\"statements\":", stdout);
            print_statement_list(&stmt->as.case_stmt.statements);
            putchar('}');
            break;

        case STMT_DEFAULT:
            printf("{\"kind\":\"default\",\"line\":%d,\"column\":%d,\"statements\":",
                   stmt->line, stmt->column);
            print_statement_list(&stmt->as.default_stmt.statements);
            putchar('}');
            break;

        case STMT_BREAK:
            printf("{\"kind\":\"break\",\"line\":%d,\"column\":%d,\"isValidContext\":%s}",
                   stmt->line, stmt->column,
                   stmt->as.break_stmt.is_valid_context ? "true" : "false");
            break;

        case STMT_BLOCK:
            printf("{\"kind\":\"block\",\"line\":%d,\"column\":%d,\"statements\":",
                   stmt->line, stmt->column);
            print_statement_list(&stmt->as.block.statements);
            putchar('}');
            break;

        case STMT_ASSIGN:
            printf("{\"kind\":\"assign\",\"line\":%d,\"column\":%d,\"name\":",
                   stmt->line, stmt->column);
            json_string(stmt->as.assign.name);
            fputs(",\"value\":", stdout);
            print_expression(stmt->as.assign.value);
            putchar('}');
            break;

        case STMT_VAR_DECL:
            printf("{\"kind\":\"var_decl\",\"line\":%d,\"column\":%d,\"typeName\":",
                   stmt->line, stmt->column);
            json_string(stmt->as.var_decl.type_name);
            fputs(",\"name\":", stdout);
            json_string(stmt->as.var_decl.name);
            fputs(",\"init\":", stdout);
            print_expression(stmt->as.var_decl.init);
            putchar('}');
            break;

        case STMT_EXPR:
            printf("{\"kind\":\"expr_stmt\",\"line\":%d,\"column\":%d,\"expr\":",
                   stmt->line, stmt->column);
            print_expression(stmt->as.expr_stmt.expr);
            putchar('}');
            break;

        case STMT_EMPTY:
            printf("{\"kind\":\"empty\",\"line\":%d,\"column\":%d}", stmt->line, stmt->column);
            break;
    }
}

static void print_program(const Program *program)
{
    if (program == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"kind\":\"program\",\"line\":%d,\"column\":%d,\"statements\":",
           program->line, program->column);
    print_statement_list(&program->statements);
    putchar('}');
}

static void collect_switches(const Statement *stmt, int *first)
{
    int i;
    if (stmt == NULL) return;
    if (stmt->kind == STMT_SWITCH) {
        if (!*first) putchar(',');
        *first = 0;
        char *cond_str = expr_to_string(stmt->as.switch_stmt.condition);
        printf("{\"line\":%d,\"column\":%d,\"condition\":", stmt->line, stmt->column);
        json_string(cond_str);
        free(cond_str);
        printf(",\"caseCount\":%d,\"cases\":[", stmt->as.switch_stmt.case_entries.count);
        for (i = 0; i < stmt->as.switch_stmt.case_entries.count; i++) {
            CaseEntry *e = &stmt->as.switch_stmt.case_entries.items[i];
            if (i > 0) putchar(',');
            printf("{\"line\":%d,\"column\":%d,\"constValue\":%lld,\"isValidConst\":%s,\"isDuplicate\":%s,\"prevLine\":%d,\"prevColumn\":%d,\"exprText\":",
                   e->line, e->column, (long long)e->const_val,
                   e->is_valid_const ? "true" : "false",
                   e->is_duplicate ? "true" : "false",
                   e->prev_line, e->prev_column);
            json_string(e->expr_text);
            putchar('}');
        }
        printf("],\"hasDefault\":%s,\"defaultLine\":%d,\"defaultColumn\":%d}",
               stmt->as.switch_stmt.has_default ? "true" : "false",
               stmt->as.switch_stmt.default_line,
               stmt->as.switch_stmt.default_column);

        if (stmt->as.switch_stmt.body != NULL) {
            collect_switches(stmt->as.switch_stmt.body, first);
        }
        return;
    }

    if (stmt->kind == STMT_BLOCK) {
        for (i = 0; i < stmt->as.block.statements.count; i++) {
            collect_switches(stmt->as.block.statements.items[i], first);
        }
    } else if (stmt->kind == STMT_CASE) {
        for (i = 0; i < stmt->as.case_stmt.statements.count; i++) {
            collect_switches(stmt->as.case_stmt.statements.items[i], first);
        }
    } else if (stmt->kind == STMT_DEFAULT) {
        for (i = 0; i < stmt->as.default_stmt.statements.count; i++) {
            collect_switches(stmt->as.default_stmt.statements.items[i], first);
        }
    }
}

static void print_switch_analysis(const Program *program)
{
    int i;
    int first = 1;
    fputs("{\"switches\":[", stdout);
    if (program != NULL) {
        for (i = 0; i < program->statements.count; i++) {
            collect_switches(program->statements.items[i], &first);
        }
    }
    fputs("]}", stdout);
}

static void print_diagnostics(const DiagnosticList *diagnostics, char filter_prefix)
{
    int index;
    int count = 0;
    putchar('[');
    for (index = 0; index < diagnostics->count; index++) {
        const Diagnostic *item = &diagnostics->items[index];
        if (filter_prefix != '\0' && item->code[0] != filter_prefix) {
            continue;
        }
        if (count > 0) {
            putchar(',');
        }
        count++;
        printf("{\"code\":");
        json_string(item->code);
        printf(",\"line\":%d,\"column\":%d,\"message\":", item->line, item->column);
        json_string(item->message);
        putchar('}');
    }
    putchar(']');
}

static const Diagnostic *first_diagnostic(const DiagnosticList *diagnostics, char prefix)
{
    int index;
    for (index = 0; index < diagnostics->count; index++) {
        if (diagnostics->items[index].code[0] == prefix) {
            return &diagnostics->items[index];
        }
    }
    return NULL;
}

void json_print_result(const Program *program, const DiagnosticList *diagnostics,
                       int syntax_ok, int lex_errors)
{
    int index;
    int semantic_errors = 0;
    int first_token = 1;
    const Diagnostic *syntax_diagnostic;
    const char *stage;

    for (index = 0; index < diagnostics->count; index++) {
        if (diagnostics->items[index].code[0] == 'S') {
            semantic_errors++;
        }
    }

    if (!syntax_ok) {
        stage = lex_errors > 0 ? "lexical" : "syntax";
    } else if (semantic_errors > 0) {
        stage = "semantic";
    } else {
        stage = "valid";
    }

    printf("{\"ok\":%s,\"stage\":", (syntax_ok && semantic_errors == 0) ? "true" : "false");
    json_string(stage);
    fputs(",\"tokens\":[", stdout);
    for (index = 0; index < g_json_tokens.count; index++) {
        JsonToken *token = &g_json_tokens.items[index];
        if (!first_token) {
            putchar(',');
        }
        first_token = 0;
        printf("{\"index\":%d,\"type\":", index + 1);
        json_string(token->type);
        fputs(",\"lexeme\":", stdout);
        json_string(token->lexeme);
        printf(",\"line\":%d,\"column\":%d}", token->line, token->column);
    }
    fputs("],\"diagnostics\":", stdout);
    print_diagnostics(diagnostics, '\0');
    fputs(",\"syntax\":{\"ok\":", stdout);
    fputs(syntax_ok ? "true" : "false", stdout);
    fputs(",\"stage\":", stdout);
    if (syntax_ok) {
        fputs("null", stdout);
    } else {
        json_string(lex_errors > 0 ? "lexical" : "syntax");
    }
    fputs(",\"error\":", stdout);
    if (syntax_ok) {
        fputs("null", stdout);
    } else {
        syntax_diagnostic = first_diagnostic(diagnostics, lex_errors > 0 ? 'L' : 'P');
        if (syntax_diagnostic == NULL) {
            syntax_diagnostic = first_diagnostic(diagnostics, 'L');
        }
        json_string(syntax_diagnostic != NULL ? syntax_diagnostic->message : "syntax analysis failed");
    }
    fputs("},\"ast\":", stdout);
    if (syntax_ok) {
        print_program(program);
    } else {
        fputs("null", stdout);
    }
    fputs(",\"switchAnalysis\":", stdout);
    if (syntax_ok) {
        print_switch_analysis(program);
    } else {
        fputs("{\"switches\":[]}", stdout);
    }
    fputs(",\"semantics\":{\"ok\":", stdout);
    fputs((syntax_ok && semantic_errors == 0) ? "true" : "false", stdout);
    fputs(",\"diagnostics\":", stdout);
    print_diagnostics(diagnostics, 'S');
    fputs("}}\n", stdout);
}
