%{
#include "switchc.h"
#include "parser.tab.h"
#include "driver.h"
#include "json_output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_line = 1;
int g_column = 1;
int g_token_line = 1;
int g_token_column = 1;
int g_token_count = 0;
static int g_token_index = 0;
static int comment_start_line = 1;
static int comment_start_column = 1;

static void update_position(const char *text, int length)
{
    int index;
    for (index = 0; index < length; index++) {
        if (text[index] == '\n') {
            g_line++;
            g_column = 1;
        } else {
            g_column++;
        }
    }
}

static int keyword_code(const char *text)
{
    if (strcmp(text, "switch") == 0) return KW_SWITCH;
    if (strcmp(text, "case") == 0) return KW_CASE;
    if (strcmp(text, "default") == 0) return KW_DEFAULT;
    if (strcmp(text, "break") == 0) return KW_BREAK;
    if (strcmp(text, "int") == 0) return KW_INT;
    if (strcmp(text, "char") == 0) return KW_CHAR;
    if (strcmp(text, "float") == 0) return KW_FLOAT;
    if (strcmp(text, "void") == 0) return KW_VOID;
    return 0;
}

static const char *keyword_name(int token)
{
    switch (token) {
        case KW_SWITCH: return "SWITCH";
        case KW_CASE: return "CASE";
        case KW_DEFAULT: return "DEFAULT";
        case KW_BREAK: return "BREAK";
        case KW_INT: return "INT";
        case KW_CHAR: return "CHAR";
        case KW_FLOAT: return "FLOAT";
        case KW_VOID: return "VOID";
        case IDENT: return "IDENTIFIER";
        case INT_LITERAL: return "INT_LITERAL";
        case FLOAT_LITERAL: return "FLOAT_LITERAL";
        case CHAR_LITERAL: return "CHAR_LITERAL";
        case STRING_LITERAL: return "STRING_LITERAL";
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        case STAR: return "STAR";
        case SLASH: return "SLASH";
        case PERCENT: return "PERCENT";
        case ASSIGN: return "ASSIGN";
        case EQ_EQ: return "EQUAL_EQUAL";
        case NE: return "NOT_EQUAL";
        case LT: return "LESS";
        case LE: return "LESS_EQUAL";
        case GT: return "GREATER";
        case GE: return "GREATER_EQUAL";
        case AMP_AMP: return "LOGICAL_AND";
        case PIPE_PIPE: return "LOGICAL_OR";
        case EXCLAM: return "LOGICAL_NOT";
        case AMP: return "BIT_AND";
        case PIPE: return "BIT_OR";
        case CARET: return "BIT_XOR";
        case TILDE: return "BIT_NOT";
        case SHL: return "SHIFT_LEFT";
        case SHR: return "SHIFT_RIGHT";
        case LPAREN: return "LEFT_PAREN";
        case RPAREN: return "RIGHT_PAREN";
        case LBRACE: return "LEFT_BRACE";
        case RBRACE: return "RIGHT_BRACE";
        case COLON: return "COLON";
        case SEMI: return "SEMICOLON";
        case COMMA: return "COMMA";
        default: return "TOKEN";
    }
}

static char parse_char_literal(const char *text)
{
    if (text[1] == '\\') {
        switch (text[2]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            default: return text[2];
        }
    }
    return text[1];
}

static char *unquote_string(const char *text)
{
    int length = (int)strlen(text);
    char *result;
    int index = 1;
    int out = 0;
    if (length < 2) return duplicate_string("");
    result = (char *)malloc((size_t)length + 1);
    while (index < length - 1) {
        if (text[index] == '\\' && index + 1 < length - 1) {
            index++;
            switch (text[index]) {
                case 'n': result[out++] = '\n'; break;
                case 't': result[out++] = '\t'; break;
                case 'r': result[out++] = '\r'; break;
                case '0': result[out++] = '\0'; break;
                case '\\': result[out++] = '\\'; break;
                case '"': result[out++] = '"'; break;
                default: result[out++] = text[index]; break;
            }
        } else {
            result[out++] = text[index];
        }
        index++;
    }
    result[out] = '\0';
    return result;
}

void print_token_line(const char *type, const char *text, int line, int column)
{
    if (g_token_index == 0) {
        printf("%-4s %-16s %-22s %-6s %-6s\n", "#", "TOKEN", "LEXEME", "LINE", "COL");
        printf("%-4s %-16s %-22s %-6s %-6s\n", "---", "-----", "------", "----", "---");
    }
    g_token_index++;
    printf("%-4d %-16s %-22s %-6d %-6d\n", g_token_index, type, text, line, column);
}

#define YY_USER_ACTION do { \
    yylloc.first_line = g_line; \
    yylloc.first_column = g_column; \
    g_token_line = g_line; \
    g_token_column = g_column; \
    update_position(yytext, yyleng); \
    yylloc.last_line = g_line; \
    yylloc.last_column = g_column > 1 ? g_column - 1 : 1; \
} while (0);

#define EMIT(token, name) do { \
    g_token_count++; \
    json_tokens_add((name), yytext, yylloc.first_line, yylloc.first_column); \
    if (g_show_tokens) { \
        print_token_line((name), yytext, yylloc.first_line, yylloc.first_column); \
    } \
    return (token); \
} while (0)
%}

%option noyywrap nounput noinput

%x COMMENT

%%

"/*" {
    comment_start_line = yylloc.first_line;
    comment_start_column = yylloc.first_column;
    BEGIN(COMMENT);
}

"//"[^\n]* { }

[ \t\r\n]+ { }

\'([^'\\\n]|\\.)\' {
    yylval.char_val.value = parse_char_literal(yytext);
    yylval.char_val.raw = duplicate_string(yytext);
    EMIT(CHAR_LITERAL, "CHAR_LITERAL");
}

\'([^'\\\n]|\\.)* {
    diag_add(g_diagnostics, "L002", yylloc.first_line, yylloc.first_column,
             "unterminated character literal");
    yylval.char_val.value = parse_char_literal(yytext);
    yylval.char_val.raw = duplicate_string(yytext);
    EMIT(CHAR_LITERAL, "CHAR_LITERAL");
}

\"([^"\\\n]|\\.)*\" {
    yylval.text = unquote_string(yytext);
    EMIT(STRING_LITERAL, "STRING_LITERAL");
}

\"([^"\\\n]|\\.)* {
    diag_add(g_diagnostics, "L002", yylloc.first_line, yylloc.first_column,
             "unterminated string literal");
    yylval.text = unquote_string(yytext);
    EMIT(STRING_LITERAL, "STRING_LITERAL");
}

0[xX][0-9a-fA-F]+ {
    yylval.int_val.value = strtoll(yytext, NULL, 16);
    yylval.int_val.raw = duplicate_string(yytext);
    EMIT(INT_LITERAL, "INT_LITERAL");
}

0[0-7]+ {
    yylval.int_val.value = strtoll(yytext, NULL, 8);
    yylval.int_val.raw = duplicate_string(yytext);
    EMIT(INT_LITERAL, "INT_LITERAL");
}

[0-9]+\.[0-9]+([eE][+-]?[0-9]+)? {
    yylval.float_val.value = strtod(yytext, NULL);
    yylval.float_val.raw = duplicate_string(yytext);
    EMIT(FLOAT_LITERAL, "FLOAT_LITERAL");
}

[0-9]+ {
    yylval.int_val.value = strtoll(yytext, NULL, 10);
    yylval.int_val.raw = duplicate_string(yytext);
    EMIT(INT_LITERAL, "INT_LITERAL");
}

[A-Za-z_][A-Za-z0-9_]* {
    int kw = keyword_code(yytext);
    if (kw != 0) {
        EMIT(kw, keyword_name(kw));
    }
    yylval.text = duplicate_string(yytext);
    EMIT(IDENT, "IDENTIFIER");
}

"==" { EMIT(EQ_EQ, "EQUAL_EQUAL"); }
"!=" { EMIT(NE, "NOT_EQUAL"); }
"<=" { EMIT(LE, "LESS_EQUAL"); }
">=" { EMIT(GE, "GREATER_EQUAL"); }
"&&" { EMIT(AMP_AMP, "LOGICAL_AND"); }
"||" { EMIT(PIPE_PIPE, "LOGICAL_OR"); }
"<<" { EMIT(SHL, "SHIFT_LEFT"); }
">>" { EMIT(SHR, "SHIFT_RIGHT"); }

"=" { EMIT(ASSIGN, "ASSIGN"); }
"<" { EMIT(LT, "LESS"); }
">" { EMIT(GT, "GREATER"); }
"+" { EMIT(PLUS, "PLUS"); }
"-" { EMIT(MINUS, "MINUS"); }
"*" { EMIT(STAR, "STAR"); }
"/" { EMIT(SLASH, "SLASH"); }
"%" { EMIT(PERCENT, "PERCENT"); }
"!" { EMIT(EXCLAM, "LOGICAL_NOT"); }
"&" { EMIT(AMP, "BIT_AND"); }
"|" { EMIT(PIPE, "BIT_OR"); }
"^" { EMIT(CARET, "BIT_XOR"); }
"~" { EMIT(TILDE, "BIT_NOT"); }

"{" { EMIT(LBRACE, "LEFT_BRACE"); }
"}" { EMIT(RBRACE, "RIGHT_BRACE"); }
"(" { EMIT(LPAREN, "LEFT_PAREN"); }
")" { EMIT(RPAREN, "RIGHT_PAREN"); }
":" { EMIT(COLON, "COLON"); }
";" { EMIT(SEMI, "SEMICOLON"); }
"," { EMIT(COMMA, "COMMA"); }

<COMMENT>"*/" { BEGIN(INITIAL); }
<COMMENT>\n { }
<COMMENT>. { }
<COMMENT><<EOF>> {
    diag_add(g_diagnostics, "L003", comment_start_line, comment_start_column,
             "unterminated block comment");
    BEGIN(INITIAL);
    return 0;
}

<<EOF>> {
    json_tokens_add("EOF", "EOF", g_line, g_column);
    if (g_show_tokens) {
        print_token_line("EOF", "EOF", g_line, g_column);
    }
    return 0;
}

. {
    diag_add(g_diagnostics, "L001", yylloc.first_line, yylloc.first_column,
             "illegal character '%c' (0x%02X)", yytext[0], (unsigned char)yytext[0]);
}

%%
