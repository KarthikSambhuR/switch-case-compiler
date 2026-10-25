#ifndef SWITCHC_H
#define SWITCHC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SWITCHC_MAX_MESSAGE 256
#define SWITCHC_MAX_CODE 8

typedef struct {
    char code[SWITCHC_MAX_CODE];
    int line;
    int column;
    char message[SWITCHC_MAX_MESSAGE];
} Diagnostic;

typedef struct {
    Diagnostic *items;
    int count;
    int capacity;
} DiagnosticList;

void diag_init(DiagnosticList *list);
void diag_free(DiagnosticList *list);
void diag_add(DiagnosticList *list, const char *code, int line, int column, const char *format, ...);
void diag_print(const DiagnosticList *list);

void *allocate_memory(size_t size);
void *resize_memory(void *pointer, size_t size);
char *duplicate_string(const char *value);

/* Expressions */
typedef enum {
    EXPR_INTEGER_LITERAL,
    EXPR_FLOAT_LITERAL,
    EXPR_CHAR_LITERAL,
    EXPR_STRING_LITERAL,
    EXPR_IDENTIFIER,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_PAREN
} ExpressionKind;

typedef enum {
    UNARY_PLUS,
    UNARY_MINUS,
    UNARY_BITWISE_NOT,
    UNARY_LOGICAL_NOT
} UnaryOp;

typedef enum {
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
    BIN_MOD,
    BIN_EQ,
    BIN_NE,
    BIN_LT,
    BIN_LE,
    BIN_GT,
    BIN_GE,
    BIN_BIT_AND,
    BIN_BIT_OR,
    BIN_BIT_XOR,
    BIN_SHL,
    BIN_SHR,
    BIN_LOG_AND,
    BIN_LOG_OR
} BinaryOp;

typedef struct Expression Expression;

struct Expression {
    ExpressionKind kind;
    int line;
    int column;
    union {
        struct {
            int64_t value;
            char *raw_text;
        } integer_lit;
        struct {
            double value;
            char *raw_text;
        } float_lit;
        struct {
            char value;
            char *raw_text;
        } char_lit;
        struct {
            char *text;
        } string_lit;
        struct {
            char *name;
        } identifier;
        struct {
            UnaryOp op;
            Expression *operand;
        } unary;
        struct {
            BinaryOp op;
            Expression *left;
            Expression *right;
        } binary;
        struct {
            Expression *inner;
        } paren;
    } as;
};

/* Statements */
typedef enum {
    STMT_SWITCH,
    STMT_CASE,
    STMT_DEFAULT,
    STMT_BREAK,
    STMT_BLOCK,
    STMT_EXPR,
    STMT_ASSIGN,
    STMT_VAR_DECL,
    STMT_EMPTY
} StatementKind;

typedef struct Statement Statement;
typedef struct StatementList StatementList;

struct StatementList {
    Statement **items;
    int count;
    int capacity;
};

/* Case label record for switch validation */
typedef struct CaseEntry {
    int64_t const_val;
    int is_valid_const;
    int line;
    int column;
    char *expr_text;
    int is_duplicate;
    int prev_line;
    int prev_column;
} CaseEntry;

typedef struct CaseEntryList {
    CaseEntry *items;
    int count;
    int capacity;
} CaseEntryList;

struct Statement {
    StatementKind kind;
    int line;
    int column;
    union {
        struct {
            Expression *condition;
            Statement *body;
            CaseEntryList case_entries;
            int has_default;
            int default_line;
            int default_column;
            int multiple_defaults;
        } switch_stmt;
        struct {
            Expression *expr;
            StatementList statements;
            int64_t const_val;
            int is_valid_const;
            int is_duplicate;
        } case_stmt;
        struct {
            StatementList statements;
        } default_stmt;
        struct {
            int is_valid_context;
        } break_stmt;
        struct {
            StatementList statements;
        } block;
        struct {
            char *name;
            Expression *value;
        } assign;
        struct {
            char *type_name;
            char *name;
            Expression *init;
        } var_decl;
        struct {
            Expression *expr;
        } expr_stmt;
    } as;
};

typedef struct {
    StatementList statements;
    int line;
    int column;
} Program;

/* AST constructors & free */
Expression *expr_create_integer(int64_t val, const char *raw, int line, int col);
Expression *expr_create_float(double val, const char *raw, int line, int col);
Expression *expr_create_char(char val, const char *raw, int line, int col);
Expression *expr_create_string(const char *text, int line, int col);
Expression *expr_create_identifier(const char *name, int line, int col);
Expression *expr_create_unary(UnaryOp op, Expression *operand, int line, int col);
Expression *expr_create_binary(BinaryOp op, Expression *left, Expression *right, int line, int col);
Expression *expr_create_paren(Expression *inner, int line, int col);
void expr_free(Expression *expr);
char *expr_to_string(const Expression *expr);

Statement *stmt_create_switch(Expression *cond, Statement *body, int line, int col);
Statement *stmt_create_case(Expression *expr, int line, int col);
Statement *stmt_create_default(int line, int col);
Statement *stmt_create_break(int line, int col);
Statement *stmt_create_block(int line, int col);
Statement *stmt_create_assign(const char *name, Expression *val, int line, int col);
Statement *stmt_create_var_decl(const char *type_name, const char *name, Expression *init, int line, int col);
Statement *stmt_create_expr(Expression *expr, int line, int col);
Statement *stmt_create_empty(int line, int col);

void stmt_list_init(StatementList *list);
void stmt_list_add(StatementList *list, Statement *stmt);
void stmt_list_free(StatementList *list);
void stmt_free(Statement *stmt);
void program_free(Program *prog);

/* Semantic Analysis */
int semantic_validate_program(Program *program, DiagnosticList *diagnostics);

/* Constant Evaluation */
typedef struct {
    int is_constant;
    int64_t value;
    const char *error_msg;
    int err_line;
    int err_column;
} ConstEvalResult;

ConstEvalResult eval_constant_expression(const Expression *expr);

#endif /* SWITCHC_H */
