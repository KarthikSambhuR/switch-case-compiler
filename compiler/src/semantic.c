#include "switchc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Switch Statement Compiler - Semantic Analysis Module
 *
 * This file performs semantic validation on the Abstract Syntax Tree (AST):
 *   1. Evaluates case label expressions at compile-time (constant folding).
 *   2. Detects duplicate constant case labels within the same switch.
 *   3. Enforces that 'case', 'default', and 'break' only occur inside a switch.
 *   4. Ensures only one 'default' label exists per switch statement.
 *   5. Ensures switch condition is an integral type (rejects floats).
 * ========================================================================= */

/* =========================================================================
 * PART 1: Compile-Time Constant Expression Evaluation (Constant Folding)
 * ========================================================================= */

/* Computes a binary math/bitwise/comparison operation between two constant integers */
static int evaluate_binary_op(BinaryOp op, int64_t a, int64_t b, int64_t *result, const char **error_msg)
{
    switch (op) {
        /* Arithmetic */
        case BIN_ADD: *result = a + b; return 1;
        case BIN_SUB: *result = a - b; return 1;
        case BIN_MUL: *result = a * b; return 1;
        case BIN_DIV:
            if (b == 0) {
                *error_msg = "Division by zero in case label constant expression";
                return 0;
            }
            *result = a / b;
            return 1;
        case BIN_MOD:
            if (b == 0) {
                *error_msg = "Modulo by zero in case label constant expression";
                return 0;
            }
            *result = a % b;
            return 1;

        /* Relational & Equality */
        case BIN_EQ:  *result = (a == b); return 1;
        case BIN_NE:  *result = (a != b); return 1;
        case BIN_LT:  *result = (a < b);  return 1;
        case BIN_LE:  *result = (a <= b); return 1;
        case BIN_GT:  *result = (a > b);  return 1;
        case BIN_GE:  *result = (a >= b); return 1;

        /* Bitwise operations */
        case BIN_BIT_AND: *result = a & b;  return 1;
        case BIN_BIT_OR:  *result = a | b;  return 1;
        case BIN_BIT_XOR: *result = a ^ b;  return 1;
        case BIN_SHL:     *result = a << b; return 1;
        case BIN_SHR:     *result = a >> b; return 1;

        /* Logical operations */
        case BIN_LOG_AND: *result = (a && b); return 1;
        case BIN_LOG_OR:  *result = (a || b); return 1;

        default:
            *error_msg = "Unsupported binary operator in case label";
            return 0;
    }
}

/* Computes a unary operation on an integer constant */
static int evaluate_unary_op(UnaryOp op, int64_t a, int64_t *result)
{
    switch (op) {
        case UNARY_PLUS:        *result = +a; return 1;
        case UNARY_MINUS:       *result = -a; return 1;
        case UNARY_BITWISE_NOT: *result = ~a; return 1;
        case UNARY_LOGICAL_NOT: *result = !a; return 1;
        default: return 0;
    }
}

/* Evaluates an expression AST node down to a constant integer value at compile time.
 * Returns is_constant = 1 with value, or is_constant = 0 with a clear error message. */
ConstEvalResult eval_constant_expression(const Expression *expr)
{
    ConstEvalResult result;
    result.is_constant = 0;
    result.value = 0;
    result.error_msg = NULL;
    result.err_line = expr ? expr->line : 0;
    result.err_column = expr ? expr->column : 0;

    if (expr == NULL) {
        result.error_msg = "Missing expression in case label";
        return result;
    }

    switch (expr->kind) {
        /* Direct Integer (decimal, hex 0x, octal 0) */
        case EXPR_INTEGER_LITERAL:
            result.is_constant = 1;
            result.value = expr->as.integer_lit.value;
            return result;

        /* Character literal ('A' -> ASCII 65) */
        case EXPR_CHAR_LITERAL:
            result.is_constant = 1;
            result.value = (int64_t)(unsigned char)expr->as.char_lit.value;
            return result;

        /* Parentheses: (expr) */
        case EXPR_PAREN:
            return eval_constant_expression(expr->as.paren.inner);

        /* Unary: +expr, -expr, ~expr, !expr */
        case EXPR_UNARY: {
            ConstEvalResult inner = eval_constant_expression(expr->as.unary.operand);
            if (!inner.is_constant) return inner;

            evaluate_unary_op(expr->as.unary.op, inner.value, &result.value);
            result.is_constant = 1;
            return result;
        }

        /* Binary: left + right, left * right, etc. */
        case EXPR_BINARY: {
            ConstEvalResult left = eval_constant_expression(expr->as.binary.left);
            if (!left.is_constant) return left;

            ConstEvalResult right = eval_constant_expression(expr->as.binary.right);
            if (!right.is_constant) return right;

            if (!evaluate_binary_op(expr->as.binary.op, left.value, right.value, &result.value, &result.error_msg)) {
                result.err_line = expr->line;
                result.err_column = expr->column;
                return result;
            }
            result.is_constant = 1;
            return result;
        }

        /* Non-constant or non-integer errors */
        case EXPR_IDENTIFIER:
            result.error_msg = "Case label expression must be a compile-time constant; non-constant identifier is not allowed";
            return result;

        case EXPR_FLOAT_LITERAL:
            result.error_msg = "Case label must be an integer constant expression; floating-point value is not allowed";
            return result;

        case EXPR_STRING_LITERAL:
            result.error_msg = "Case label must be an integer constant expression; string literal is not allowed";
            return result;
    }

    result.error_msg = "Unsupported expression in case label";
    return result;
}

/* =========================================================================
 * PART 2: Switch Scope & Case Table Helpers
 * ========================================================================= */

/* Stack node to keep track of nested switch statements */
typedef struct SwitchScope {
    Statement *switch_stmt;
    struct SwitchScope *outer;
} SwitchScope;

/* Checks if a constant value has already been registered in the current switch.
 * Returns 1 if duplicate found (and sets prev_line, prev_col), or 0 if unique. */
static int check_duplicate_case(Statement *switch_stmt, int64_t value, int *prev_line, int *prev_col)
{
    CaseEntryList *table = &switch_stmt->as.switch_stmt.case_entries;
    int i;
    for (i = 0; i < table->count; i++) {
        CaseEntry *entry = &table->items[i];
        if (entry->is_valid_const && entry->const_val == value) {
            *prev_line = entry->line;
            *prev_col = entry->column;
            return 1; /* Duplicate found! */
        }
    }
    return 0; /* Unique */
}

/* Adds a case entry into the switch's internal case table */
static void add_case_to_switch(Statement *switch_stmt, int64_t val, int is_const,
                               int line, int col, const char *text,
                               int is_dup, int prev_line, int prev_col)
{
    CaseEntryList *list = &switch_stmt->as.switch_stmt.case_entries;

    /* Expand array dynamically if needed */
    if (list->count == list->capacity) {
        int new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        list->items = (CaseEntry *)resize_memory(list->items, (size_t)new_capacity * sizeof(CaseEntry));
        list->capacity = new_capacity;
    }

    CaseEntry *entry = &list->items[list->count++];
    entry->const_val = val;
    entry->is_valid_const = is_const;
    entry->line = line;
    entry->column = col;
    entry->expr_text = duplicate_string(text);
    entry->is_duplicate = is_dup;
    entry->prev_line = prev_line;
    entry->prev_column = prev_col;
}

/* Forward declarations for recursive AST traversal */
static void validate_statement(Statement *stmt, SwitchScope *scope, DiagnosticList *diags);

static void validate_statements(StatementList *list, SwitchScope *scope, DiagnosticList *diags)
{
    int i;
    if (list == NULL) return;
    for (i = 0; i < list->count; i++) {
        validate_statement(list->items[i], scope, diags);
    }
}

/* =========================================================================
 * PART 3: Individual Statement Validators
 * ========================================================================= */

/* Validates a 'case <expr>:' statement:
 *   1. Must be inside an enclosing switch (Rule S007)
 *   2. Expression must be a compile-time constant integer (Rule S001)
 *   3. Expression value must not duplicate an earlier case label (Rule S004)
 */
static void validate_case_statement(Statement *stmt, SwitchScope *scope, DiagnosticList *diags)
{
    char *text = expr_to_string(stmt->as.case_stmt.expr);

    /* Step 1: Check switch context */
    if (scope == NULL) {
        diag_add(diags, "S007", stmt->line, stmt->column,
                 "'case' label not within a switch statement");
        free(text);
        return;
    }

    /* Step 2: Evaluate expression to a constant integer */
    ConstEvalResult eval = eval_constant_expression(stmt->as.case_stmt.expr);

    if (!eval.is_constant) {
        /* Not a compile-time integer constant (variable, float, zero division) */
        stmt->as.case_stmt.is_valid_const = 0;
        diag_add(diags, "S001", eval.err_line, eval.err_column,
                 "%s", eval.error_msg ? eval.error_msg : "Invalid case label expression");
        add_case_to_switch(scope->switch_stmt, 0, 0, stmt->line, stmt->column, text, 0, 0, 0);
        free(text);
        validate_statements(&stmt->as.case_stmt.statements, scope, diags);
        return;
    }

    /* Constant evaluation succeeded */
    stmt->as.case_stmt.is_valid_const = 1;
    stmt->as.case_stmt.const_val = eval.value;

    /* Step 3: Check for duplicate constant value in this switch */
    int prev_line = 0;
    int prev_col = 0;
    int is_dup = check_duplicate_case(scope->switch_stmt, eval.value, &prev_line, &prev_col);

    if (is_dup) {
        stmt->as.case_stmt.is_duplicate = 1;
        diag_add(diags, "S004", stmt->line, stmt->column,
                 "Duplicate case label value '%lld' (previously declared at line %d, column %d)",
                 (long long)eval.value, prev_line, prev_col);
    }

    /* Step 4: Register this case in the switch's table */
    add_case_to_switch(scope->switch_stmt, eval.value, 1, stmt->line, stmt->column,
                       text, is_dup, prev_line, prev_col);
    free(text);

    /* Step 5: Validate child statements inside this case */
    validate_statements(&stmt->as.case_stmt.statements, scope, diags);
}

/* Validates a 'default:' label statement:
 *   1. Must be inside an enclosing switch (Rule S007)
 *   2. Only one 'default' label allowed per switch (Rule S005)
 */
static void validate_default_statement(Statement *stmt, SwitchScope *scope, DiagnosticList *diags)
{
    if (scope == NULL) {
        diag_add(diags, "S007", stmt->line, stmt->column,
                 "'default' label not within a switch statement");
    } else if (scope->switch_stmt->as.switch_stmt.has_default) {
        /* Already has a default label */
        scope->switch_stmt->as.switch_stmt.multiple_defaults = 1;
        diag_add(diags, "S005", stmt->line, stmt->column,
                 "Multiple 'default' labels in switch statement (previous 'default' at line %d, column %d)",
                 scope->switch_stmt->as.switch_stmt.default_line,
                 scope->switch_stmt->as.switch_stmt.default_column);
    } else {
        /* First default seen for this switch */
        scope->switch_stmt->as.switch_stmt.has_default = 1;
        scope->switch_stmt->as.switch_stmt.default_line = stmt->line;
        scope->switch_stmt->as.switch_stmt.default_column = stmt->column;
    }

    validate_statements(&stmt->as.default_stmt.statements, scope, diags);
}

/* Validates a 'break;' statement:
 *   1. Must be inside an enclosing switch (Rule S006)
 */
static void validate_break_statement(Statement *stmt, SwitchScope *scope, DiagnosticList *diags)
{
    if (scope == NULL) {
        stmt->as.break_stmt.is_valid_context = 0;
        diag_add(diags, "S006", stmt->line, stmt->column,
                 "'break' statement not within switch statement");
    } else {
        stmt->as.break_stmt.is_valid_context = 1;
    }
}

/* Recursively validates any AST statement */
static void validate_statement(Statement *stmt, SwitchScope *scope, DiagnosticList *diags)
{
    if (stmt == NULL) return;

    switch (stmt->kind) {
        case STMT_SWITCH: {
            /* 1. Condition expression must be integral (reject float literals) */
            if (stmt->as.switch_stmt.condition != NULL) {
                if (stmt->as.switch_stmt.condition->kind == EXPR_FLOAT_LITERAL) {
                    diag_add(diags, "S008", stmt->as.switch_stmt.condition->line,
                             stmt->as.switch_stmt.condition->column,
                             "Switch expression must have integral type; floating-point expression '%s' is not allowed",
                             stmt->as.switch_stmt.condition->as.float_lit.raw_text ? stmt->as.switch_stmt.condition->as.float_lit.raw_text : "");
                }
            }

            /* 2. Push new switch onto the scope stack */
            SwitchScope new_scope;
            new_scope.switch_stmt = stmt;
            new_scope.outer = scope;

            /* 3. Validate body statements inside this switch scope */
            if (stmt->as.switch_stmt.body != NULL) {
                validate_statement(stmt->as.switch_stmt.body, &new_scope, diags);
            }
            break;
        }

        case STMT_CASE:
            validate_case_statement(stmt, scope, diags);
            break;

        case STMT_DEFAULT:
            validate_default_statement(stmt, scope, diags);
            break;

        case STMT_BREAK:
            validate_break_statement(stmt, scope, diags);
            break;

        case STMT_BLOCK:
            validate_statements(&stmt->as.block.statements, scope, diags);
            break;

        default:
            /* Expressions, declarations, assignments do not affect switch semantics */
            break;
    }
}

/* =========================================================================
 * PART 4: Public Entry Point
 * ========================================================================= */

/* Validates the entire program AST for switch semantics */
int semantic_validate_program(Program *program, DiagnosticList *diagnostics)
{
    if (program == NULL) return 1;
    validate_statements(&program->statements, NULL, diagnostics);
    return 1;
}
