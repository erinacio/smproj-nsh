#ifndef PARSER_T_INC_H
#define PARSER_T_INC_H

#include "parser.h"
#include "il_t.inc.h"

typedef struct alias_lexer_s alias_lexer_t;

typedef struct alias_lexer_s {
    const char *alias_name;
    char *input_end;
    char *curr;
    alias_lexer_t *alias_lexer;
    int peek;
    int peek_len;
    char input[];
} alias_lexer_t;

typedef struct parser_s {
    char *input_end;
    char *curr;
    parser_error_t last_error;
    int peek;
    int peek_len;
    il_list_t il_list;
    alias_lexer_t *alias_lexer;
    char input[];
} parser_t;

#define CHECK_PARSER() \
do { \
    if (parser == NULL || parser->last_error != PARSER_NO_ERROR) return NULL; \
} while (0)

#endif // PARSER_T_INC_H
