#include "switchc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Expression *expr_create_integer(int64_t val, const char *raw, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_INTEGER_LITERAL;
    expr->line = line;
    expr->column = col;
    expr->as.integer_lit.value = val;
    expr->as.integer_lit.raw_text = duplicate_string(raw);
    return expr;
}

Expression *expr_create_float(double val, const char *raw, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_FLOAT_LITERAL;
    expr->line = line;
    expr->column = col;
    expr->as.float_lit.value = val;
    expr->as.float_lit.raw_text = duplicate_string(raw);
    return expr;
}

Expression *expr_create_char(char val, const char *raw, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_CHAR_LITERAL;
    expr->line = line;
    expr->column = col;
    expr->as.char_lit.value = val;
    expr->as.char_lit.raw_text = duplicate_string(raw);
    return expr;
}

Expression *expr_create_string(const char *text, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_STRING_LITERAL;
    expr->line = line;
    expr->column = col;
    expr->as.string_lit.text = duplicate_string(text);
    return expr;
}

Expression *expr_create_identifier(const char *name, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_IDENTIFIER;
    expr->line = line;
    expr->column = col;
    expr->as.identifier.name = duplicate_string(name);
    return expr;
}

Expression *expr_create_unary(UnaryOp op, Expression *operand, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_UNARY;
    expr->line = line;
    expr->column = col;
    expr->as.unary.op = op;
    expr->as.unary.operand = operand;
    return expr;
}

Expression *expr_create_binary(BinaryOp op, Expression *left, Expression *right, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_BINARY;
    expr->line = line;
    expr->column = col;
    expr->as.binary.op = op;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

Expression *expr_create_paren(Expression *inner, int line, int col)
{
    Expression *expr = (Expression *)allocate_memory(sizeof(Expression));
    expr->kind = EXPR_PAREN;
    expr->line = line;
    expr->column = col;
    expr->as.paren.inner = inner;
    return expr;
}

void expr_free(Expression *expr)
{
    if (expr == NULL) return;
    switch (expr->kind) {
        case EXPR_INTEGER_LITERAL:
            free(expr->as.integer_lit.raw_text);
            break;
        case EXPR_FLOAT_LITERAL:
            free(expr->as.float_lit.raw_text);
            break;
        case EXPR_CHAR_LITERAL:
            free(expr->as.char_lit.raw_text);
            break;
        case EXPR_STRING_LITERAL:
            free(expr->as.string_lit.text);
            break;
        case EXPR_IDENTIFIER:
            free(expr->as.identifier.name);
            break;
        case EXPR_UNARY:
            expr_free(expr->as.unary.operand);
            break;
        case EXPR_BINARY:
            expr_free(expr->as.binary.left);
            expr_free(expr->as.binary.right);
            break;
        case EXPR_PAREN:
            expr_free(expr->as.paren.inner);
            break;
    }
    free(expr);
}

static const char *unary_symbol(UnaryOp op)
{
    switch (op) {
        case UNARY_PLUS: return "+";
        case UNARY_MINUS: return "-";
        case UNARY_BITWISE_NOT: return "~";
        case UNARY_LOGICAL_NOT: return "!";
        default: return "?";
    }
}

static const char *binary_symbol(BinaryOp op)
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

char *expr_to_string(const Expression *expr)
{
    char buffer[512];
    if (expr == NULL) {
        return duplicate_string("<null>");
    }
    switch (expr->kind) {
        case EXPR_INTEGER_LITERAL:
            if (expr->as.integer_lit.raw_text) {
                return duplicate_string(expr->as.integer_lit.raw_text);
            }
            snprintf(buffer, sizeof(buffer), "%lld", (long long)expr->as.integer_lit.value);
            return duplicate_string(buffer);
        case EXPR_FLOAT_LITERAL:
            if (expr->as.float_lit.raw_text) {
                return duplicate_string(expr->as.float_lit.raw_text);
            }
            snprintf(buffer, sizeof(buffer), "%g", expr->as.float_lit.value);
            return duplicate_string(buffer);
        case EXPR_CHAR_LITERAL:
            if (expr->as.char_lit.raw_text) {
                return duplicate_string(expr->as.char_lit.raw_text);
            }
            snprintf(buffer, sizeof(buffer), "'%c'", expr->as.char_lit.value);
            return duplicate_string(buffer);
        case EXPR_STRING_LITERAL:
            snprintf(buffer, sizeof(buffer), "\"%s\"", expr->as.string_lit.text ? expr->as.string_lit.text : "");
            return duplicate_string(buffer);
        case EXPR_IDENTIFIER:
            return duplicate_string(expr->as.identifier.name ? expr->as.identifier.name : "<id>");
        case EXPR_UNARY: {
            char *op_str = expr_to_string(expr->as.unary.operand);
            snprintf(buffer, sizeof(buffer), "%s%s", unary_symbol(expr->as.unary.op), op_str);
            free(op_str);
            return duplicate_string(buffer);
        }
        case EXPR_BINARY: {
            char *l_str = expr_to_string(expr->as.binary.left);
            char *r_str = expr_to_string(expr->as.binary.right);
            snprintf(buffer, sizeof(buffer), "%s %s %s", l_str, binary_symbol(expr->as.binary.op), r_str);
            free(l_str);
            free(r_str);
            return duplicate_string(buffer);
        }
        case EXPR_PAREN: {
            char *inner_str = expr_to_string(expr->as.paren.inner);
            snprintf(buffer, sizeof(buffer), "(%s)", inner_str);
            free(inner_str);
            return duplicate_string(buffer);
        }
    }
    return duplicate_string("<unknown>");
}

void stmt_list_init(StatementList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void stmt_list_add(StatementList *list, Statement *stmt)
{
    if (stmt == NULL) return;
    if (list->count == list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = (Statement **)resize_memory(list->items, (size_t)new_capacity * sizeof(Statement *));
        list->capacity = new_capacity;
    }
    list->items[list->count++] = stmt;
}

void stmt_list_free(StatementList *list)
{
    int i;
    if (list == NULL || list->items == NULL) return;
    for (i = 0; i < list->count; i++) {
        stmt_free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

Statement *stmt_create_switch(Expression *cond, Statement *body, int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_SWITCH;
    stmt->line = line;
    stmt->column = col;
    stmt->as.switch_stmt.condition = cond;
    stmt->as.switch_stmt.body = body;
    stmt->as.switch_stmt.case_entries.items = NULL;
    stmt->as.switch_stmt.case_entries.count = 0;
    stmt->as.switch_stmt.case_entries.capacity = 0;
    stmt->as.switch_stmt.has_default = 0;
    stmt->as.switch_stmt.default_line = 0;
    stmt->as.switch_stmt.default_column = 0;
    stmt->as.switch_stmt.multiple_defaults = 0;
    return stmt;
}

Statement *stmt_create_case(Expression *expr, int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_CASE;
    stmt->line = line;
    stmt->column = col;
    stmt->as.case_stmt.expr = expr;
    stmt_list_init(&stmt->as.case_stmt.statements);
    stmt->as.case_stmt.const_val = 0;
    stmt->as.case_stmt.is_valid_const = 0;
    stmt->as.case_stmt.is_duplicate = 0;
    return stmt;
}

Statement *stmt_create_default(int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_DEFAULT;
    stmt->line = line;
    stmt->column = col;
    stmt_list_init(&stmt->as.default_stmt.statements);
    return stmt;
}

Statement *stmt_create_break(int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_BREAK;
    stmt->line = line;
    stmt->column = col;
    stmt->as.break_stmt.is_valid_context = 1;
    return stmt;
}

Statement *stmt_create_block(int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_BLOCK;
    stmt->line = line;
    stmt->column = col;
    stmt_list_init(&stmt->as.block.statements);
    return stmt;
}

Statement *stmt_create_assign(const char *name, Expression *val, int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_ASSIGN;
    stmt->line = line;
    stmt->column = col;
    stmt->as.assign.name = duplicate_string(name);
    stmt->as.assign.value = val;
    return stmt;
}

Statement *stmt_create_var_decl(const char *type_name, const char *name, Expression *init, int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_VAR_DECL;
    stmt->line = line;
    stmt->column = col;
    stmt->as.var_decl.type_name = duplicate_string(type_name);
    stmt->as.var_decl.name = duplicate_string(name);
    stmt->as.var_decl.init = init;
    return stmt;
}

Statement *stmt_create_expr(Expression *expr, int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_EXPR;
    stmt->line = line;
    stmt->column = col;
    stmt->as.expr_stmt.expr = expr;
    return stmt;
}

Statement *stmt_create_empty(int line, int col)
{
    Statement *stmt = (Statement *)allocate_memory(sizeof(Statement));
    stmt->kind = STMT_EMPTY;
    stmt->line = line;
    stmt->column = col;
    return stmt;
}

void stmt_free(Statement *stmt)
{
    int i;
    if (stmt == NULL) return;
    switch (stmt->kind) {
        case STMT_SWITCH:
            expr_free(stmt->as.switch_stmt.condition);
            stmt_free(stmt->as.switch_stmt.body);
            for (i = 0; i < stmt->as.switch_stmt.case_entries.count; i++) {
                free(stmt->as.switch_stmt.case_entries.items[i].expr_text);
            }
            free(stmt->as.switch_stmt.case_entries.items);
            break;
        case STMT_CASE:
            expr_free(stmt->as.case_stmt.expr);
            stmt_list_free(&stmt->as.case_stmt.statements);
            break;
        case STMT_DEFAULT:
            stmt_list_free(&stmt->as.default_stmt.statements);
            break;
        case STMT_BREAK:
            break;
        case STMT_BLOCK:
            stmt_list_free(&stmt->as.block.statements);
            break;
        case STMT_ASSIGN:
            free(stmt->as.assign.name);
            expr_free(stmt->as.assign.value);
            break;
        case STMT_VAR_DECL:
            free(stmt->as.var_decl.type_name);
            free(stmt->as.var_decl.name);
            expr_free(stmt->as.var_decl.init);
            break;
        case STMT_EXPR:
            expr_free(stmt->as.expr_stmt.expr);
            break;
        case STMT_EMPTY:
            break;
    }
    free(stmt);
}

void program_free(Program *prog)
{
    if (prog == NULL) return;
    stmt_list_free(&prog->statements);
    free(prog);
}
