#ifndef PARSER_H
#define PARSER_H

#include "il.h"

typedef enum parser_error_e {
    PARSER_NO_ERROR,
    PARSER_ERR_INTERNAL,
    PARSER_ERR_UNEXPECTED,
    PARSER_ERR_INCOMPLETE,
    PARSER_ERR_NOT_IMPLEMENTED,
} parser_error_t;

typedef struct parser_s parser_t;
typedef struct token_s token_t;

parser_t *parser_new(char *input_begin, char *input_end);
void parser_free(parser_t *parser);
bool parser_no_error(parser_t *parser);
const char *parser_strerror(parser_t *parser);
void parser_dump(parser_t *parser);
parser_error_t parser_error(parser_t *parser);
il_list_t *parser_il_list(parser_t *parser);

// We use LL(1)-like parser to parse input. The parameter `token` of these
// functions means peeked token of the previous or upper rule, the returned
// token means peeked token of this rule, or NULL if this rule didn't peeked.
// All these tokens should be freed only in its caller and should not be freed
// inside it.
token_t *parse_param_expand(parser_t *parser, token_t *token);
token_t *parse_subcmd_expand(parser_t *parser, token_t *token);
token_t *parse_word(parser_t *parser, token_t *token);
token_t *parse_io_redir(parser_t *parser, token_t *token);
token_t *parse_simple_cmd(parser_t *parser, token_t *token);
token_t *parse_command(parser_t *parser, token_t *token);
token_t *parse_newlines(parser_t *parser, token_t *token);
token_t *parse_pipeline(parser_t *parser, token_t *token);
token_t *parse_list(parser_t *parser, token_t *token);

#endif // PARSER_H
