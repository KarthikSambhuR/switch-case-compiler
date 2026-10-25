#ifndef DRIVER_H
#define DRIVER_H

#include "switchc.h"

extern DiagnosticList *g_diagnostics;
extern Program *g_program;
extern int g_show_tokens;
extern int g_token_line;
extern int g_token_column;
extern int g_token_count;

void print_token_line(const char *type, const char *text, int line, int column);

#endif
