#include "switchc.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *allocate_memory(size_t size)
{
    void *pointer = malloc(size == 0 ? 1 : size);
    if (pointer == NULL) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(3);
    }
    return pointer;
}

void *resize_memory(void *pointer, size_t size)
{
    void *result = realloc(pointer, size == 0 ? 1 : size);
    if (result == NULL) {
        fprintf(stderr, "fatal: out of memory\n");
        exit(3);
    }
    return result;
}

char *duplicate_string(const char *value)
{
    size_t length;
    char *copy;
    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    copy = (char *)allocate_memory(length + 1);
    memcpy(copy, value, length + 1);
    return copy;
}

char *lowercase_string(const char *value)
{
    size_t index;
    size_t length;
    char *copy;
    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    copy = (char *)allocate_memory(length + 1);
    for (index = 0; index < length; index++) {
        copy[index] = (char)tolower((unsigned char)value[index]);
    }
    copy[length] = '\0';
    return copy;
}


void diag_init(DiagnosticList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void diag_free(DiagnosticList *list)
{
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void diag_add(DiagnosticList *list, const char *code, int line, int column, const char *format, ...)
{
    Diagnostic *diagnostic;
    va_list arguments;
    if (list->count == list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = (Diagnostic *)resize_memory(list->items, (size_t)new_capacity * sizeof(Diagnostic));
        list->capacity = new_capacity;
    }
    diagnostic = &list->items[list->count++];
    snprintf(diagnostic->code, sizeof(diagnostic->code), "%s", code);
    diagnostic->line = line;
    diagnostic->column = column;
    va_start(arguments, format);
    vsnprintf(diagnostic->message, sizeof(diagnostic->message), format, arguments);
    va_end(arguments);
}

void diag_print(const DiagnosticList *list)
{
    int index;
    for (index = 0; index < list->count; index++) {
        const Diagnostic *diagnostic = &list->items[index];
        fprintf(stderr, "[%s] line %d, column %d: %s\n",
                diagnostic->code,
                diagnostic->line,
                diagnostic->column,
                diagnostic->message);
    }
}
