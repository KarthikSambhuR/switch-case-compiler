%code requires {
#include "switchc.h"
}

%code top {
#include "switchc.h"
#include "driver.h"
#include "json_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void yyerror(const char *msg);
int yylex(void);
}

%locations

%union {
    struct {
        int64_t value;
        char *raw;
    } int_val;
    struct {
        double value;
        char *raw;
    } float_val;
    struct {
        char value;
        char *raw;
    } char_val;
    char *text;
    Expression *expr;
    Statement *stmt;
    StatementList *stmt_list;
}

%token <int_val> INT_LITERAL
%token <float_val> FLOAT_LITERAL
%token <char_val> CHAR_LITERAL
%token <text> STRING_LITERAL
%token <text> IDENT

%token KW_SWITCH KW_CASE KW_DEFAULT KW_BREAK
%token KW_INT KW_CHAR KW_FLOAT KW_VOID

%token PLUS MINUS STAR SLASH PERCENT
%token ASSIGN EQ_EQ NE LT LE GT GE
%token AMP_AMP PIPE_PIPE EXCLAM
%token AMP PIPE CARET TILDE SHL SHR
%token LPAREN RPAREN LBRACE RBRACE
%token COLON SEMI COMMA

%type <text> type_specifier
%type <expr> expression primary_expression
%type <stmt> statement switch_statement labeled_statement
%type <stmt> compound_statement expression_statement jump_statement declaration_statement
%type <stmt_list> statement_list

%left PIPE_PIPE
%left AMP_AMP
%left PIPE
%left CARET
%left AMP
%left EQ_EQ NE
%left LT LE GT GE
%left SHL SHR
%left PLUS MINUS
%left STAR SLASH PERCENT
%right UMINUS UPLUS UNOT UCOMPL

%start program

%%

program:
    statement_list {
        g_program = (Program *)allocate_memory(sizeof(Program));
        g_program->statements = *$1;
        free($1);
        g_program->line = 1;
        g_program->column = 1;
    }
  | /* empty */ {
        g_program = (Program *)allocate_memory(sizeof(Program));
        stmt_list_init(&g_program->statements);
        g_program->line = 1;
        g_program->column = 1;
    }
  ;

statement_list:
    statement {
        $$ = (StatementList *)allocate_memory(sizeof(StatementList));
        stmt_list_init($$);
        stmt_list_add($$, $1);
    }
  | statement_list statement {
        $$ = $1;
        stmt_list_add($$, $2);
    }
  ;

statement:
    switch_statement
  | labeled_statement
  | compound_statement
  | expression_statement
  | jump_statement
  | declaration_statement
  | SEMI {
        $$ = stmt_create_empty(@1.first_line, @1.first_column);
    }
  ;

switch_statement:
    KW_SWITCH LPAREN expression RPAREN statement {
        $$ = stmt_create_switch($3, $5, @1.first_line, @1.first_column);
    }
  ;

labeled_statement:
    KW_CASE expression COLON {
        $$ = stmt_create_case($2, @1.first_line, @1.first_column);
    }
  | KW_DEFAULT COLON {
        $$ = stmt_create_default(@1.first_line, @1.first_column);
    }
  ;

jump_statement:
    KW_BREAK SEMI {
        $$ = stmt_create_break(@1.first_line, @1.first_column);
    }
  ;

compound_statement:
    LBRACE RBRACE {
        $$ = stmt_create_block(@1.first_line, @1.first_column);
    }
  | LBRACE statement_list RBRACE {
        $$ = stmt_create_block(@1.first_line, @1.first_column);
        $$->as.block.statements = *$2;
        free($2);
    }
  ;

declaration_statement:
    type_specifier IDENT SEMI {
        $$ = stmt_create_var_decl($1, $2, NULL, @1.first_line, @1.first_column);
        free($1);
        free($2);
    }
  | type_specifier IDENT ASSIGN expression SEMI {
        $$ = stmt_create_var_decl($1, $2, $4, @1.first_line, @1.first_column);
        free($1);
        free($2);
    }
  ;

type_specifier:
    KW_INT   { $$ = duplicate_string("int"); }
  | KW_CHAR  { $$ = duplicate_string("char"); }
  | KW_FLOAT { $$ = duplicate_string("float"); }
  | KW_VOID  { $$ = duplicate_string("void"); }
  ;

expression_statement:
    expression SEMI {
        $$ = stmt_create_expr($1, @1.first_line, @1.first_column);
    }
  | IDENT ASSIGN expression SEMI {
        $$ = stmt_create_assign($1, $3, @1.first_line, @1.first_column);
        free($1);
    }
  ;

expression:
    primary_expression
  | expression PLUS expression          { $$ = expr_create_binary(BIN_ADD, $1, $3, @2.first_line, @2.first_column); }
  | expression MINUS expression         { $$ = expr_create_binary(BIN_SUB, $1, $3, @2.first_line, @2.first_column); }
  | expression STAR expression          { $$ = expr_create_binary(BIN_MUL, $1, $3, @2.first_line, @2.first_column); }
  | expression SLASH expression         { $$ = expr_create_binary(BIN_DIV, $1, $3, @2.first_line, @2.first_column); }
  | expression PERCENT expression       { $$ = expr_create_binary(BIN_MOD, $1, $3, @2.first_line, @2.first_column); }
  | expression EQ_EQ expression         { $$ = expr_create_binary(BIN_EQ, $1, $3, @2.first_line, @2.first_column); }
  | expression NE expression            { $$ = expr_create_binary(BIN_NE, $1, $3, @2.first_line, @2.first_column); }
  | expression LT expression            { $$ = expr_create_binary(BIN_LT, $1, $3, @2.first_line, @2.first_column); }
  | expression LE expression            { $$ = expr_create_binary(BIN_LE, $1, $3, @2.first_line, @2.first_column); }
  | expression GT expression            { $$ = expr_create_binary(BIN_GT, $1, $3, @2.first_line, @2.first_column); }
  | expression GE expression            { $$ = expr_create_binary(BIN_GE, $1, $3, @2.first_line, @2.first_column); }
  | expression AMP expression           { $$ = expr_create_binary(BIN_BIT_AND, $1, $3, @2.first_line, @2.first_column); }
  | expression PIPE expression          { $$ = expr_create_binary(BIN_BIT_OR, $1, $3, @2.first_line, @2.first_column); }
  | expression CARET expression         { $$ = expr_create_binary(BIN_BIT_XOR, $1, $3, @2.first_line, @2.first_column); }
  | expression SHL expression           { $$ = expr_create_binary(BIN_SHL, $1, $3, @2.first_line, @2.first_column); }
  | expression SHR expression           { $$ = expr_create_binary(BIN_SHR, $1, $3, @2.first_line, @2.first_column); }
  | expression AMP_AMP expression       { $$ = expr_create_binary(BIN_LOG_AND, $1, $3, @2.first_line, @2.first_column); }
  | expression PIPE_PIPE expression     { $$ = expr_create_binary(BIN_LOG_OR, $1, $3, @2.first_line, @2.first_column); }
  | PLUS expression %prec UPLUS         { $$ = expr_create_unary(UNARY_PLUS, $2, @1.first_line, @1.first_column); }
  | MINUS expression %prec UMINUS       { $$ = expr_create_unary(UNARY_MINUS, $2, @1.first_line, @1.first_column); }
  | EXCLAM expression %prec UNOT        { $$ = expr_create_unary(UNARY_LOGICAL_NOT, $2, @1.first_line, @1.first_column); }
  | TILDE expression %prec UCOMPL       { $$ = expr_create_unary(UNARY_BITWISE_NOT, $2, @1.first_line, @1.first_column); }
  | LPAREN expression RPAREN            { $$ = expr_create_paren($2, @1.first_line, @1.first_column); }
  ;

primary_expression:
    INT_LITERAL    { $$ = expr_create_integer($1.value, $1.raw, @1.first_line, @1.first_column); free($1.raw); }
  | FLOAT_LITERAL  { $$ = expr_create_float($1.value, $1.raw, @1.first_line, @1.first_column); free($1.raw); }
  | CHAR_LITERAL   { $$ = expr_create_char($1.value, $1.raw, @1.first_line, @1.first_column); free($1.raw); }
  | STRING_LITERAL { $$ = expr_create_string($1, @1.first_line, @1.first_column); free($1); }
  | IDENT          { $$ = expr_create_identifier($1, @1.first_line, @1.first_column); free($1); }
  ;

%%

void yyerror(const char *msg)
{
    diag_add(g_diagnostics, "P001", yylloc.first_line, yylloc.first_column, "%s", msg);
}
