#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include "utils.h"
#include "lexer.h"
#include "parser.h"
#include "parser_t.inc.h"
#include "il.h"

parser_t *parser_new(char *input_begin, char *input_end)
{
    if (input_begin == NULL || input_end == NULL) return NULL;
    ptrdiff_t input_len = input_end - input_begin;
    if (input_len < 0) return NULL;

    parser_t *parser = malloc(sizeof(parser_t) + input_len + 1);
    if (parser == NULL) return NULL;

    memcpy(parser->input, input_begin, input_len);
    parser->input_end = parser->input + input_len;
    *parser->input_end = '\0';
    parser->curr = parser->input;
    parser->last_error = PARSER_NO_ERROR;
    parser->peek = '\0';
    parser->peek_len = 0;
    parser->alias_lexer = NULL;
    memset(&parser->il_list, 0, sizeof(il_list_t));
    if (!il_list_init(&parser->il_list)) {
        free(parser);
        return NULL;
    }

    return parser;
}

static void free_alias_lexer(alias_lexer_t *al) {
    if (al == NULL) return;
    if (al->alias_lexer != NULL) free_alias_lexer(al->alias_lexer);
    free(al);
}

void parser_free(parser_t *parser)
{
    il_list_free(&parser->il_list);
    free_alias_lexer(parser->alias_lexer);
    free(parser);
}

bool parser_no_error(parser_t *parser)
{
    return parser != NULL && parser->last_error == PARSER_NO_ERROR;
}

const char *parser_strerror(parser_t *parser)
{
    if (parser == NULL) return "Parameter is NULL";
    switch (parser->last_error) {
    case PARSER_NO_ERROR:
        return "No error";
    case PARSER_ERR_INTERNAL:
        return "Internal error";
    case PARSER_ERR_INCOMPLETE:
        return "Incomplete input";
    case PARSER_ERR_UNEXPECTED:
        return "Unexpected input";
    case PARSER_ERR_NOT_IMPLEMENTED:
        return "Feature not implemented";
    default:
        return "Unknown parser error";
    }
}

void parser_dump(parser_t *parser)
{
    printf("<<<<<===== PARSER DUMP BEGIN OF %p =====<<<<<\n", parser);
    printf("input:      %p\n", parser->input);
    printf("input_end:  %p\n", parser->input);
    printf("curr:       %p\n", parser->curr);
    printf("(index):    %lld\n", (long long)(parser->curr - parser->input));
    printf("(prec):     ");
    print_str_repr(parser->curr, 16);
    printf("\nlast_error: %s\n", parser_strerror(parser));
    printf("peek: (\\x%02x) '", parser->peek);
    print_char_repr(parser->peek);
    printf("'\npeek_len:   %d\n", parser->peek_len);
    printf(">>>>>====== PARSER DUMP END OF %p ======>>>>>\n", parser);
}

parser_error_t parser_error(parser_t *parser)
{
    return parser->last_error;
}

il_list_t *parser_il_list(parser_t *parser)
{
    return &parser->il_list;
}

#define PARSER_ASSERT_NOT_EOF(x) \
    do { \
        if (peek->type == TOKEN_EOF) { \
            parser->last_error = PARSER_ERR_INCOMPLETE; \
            return NULL; \
        } \
    } while (0)

#define PARSER_ASSERT(x) \
    do { \
        if (!(x)) { \
            parser->last_error = PARSER_ERR_UNEXPECTED; \
            return NULL; \
        } \
    } while (0)

#define PARSER_THROW(x) \
    do { \
        printf("Exception in %s (%s@%d)\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
        printf("peek = %s(\033[100m\033[97m%s\033[0m)\n", token_type_name(peek->type), peek->payload); \
        parser->last_error = (x); \
        if (peek != token) free(peek); \
        return NULL; \
    } while (0)

#define PARSER_NEXT(x) do { if (peek != token) free(peek); peek = get_token(parser, (x)); } while (0)

#define PARSER_RETURN() do { return (peek != token) ? peek : NULL; } while (0)

#define PARSER_ASSERT_NULL(x) do { if ((x) != NULL) PARSER_THROW(PARSER_ERR_UNEXPECTED); } while (0)

#define PARSER_PUSH_IL(t) il_list_push(&parser->il_list, (t))
#define PARSER_PUSH_ILs(t, p) il_list_pushs(&parser->il_list, (t), (p))
#define PARSER_PUSH_ILi(t, p) il_list_pushi(&parser->il_list, (t), (p))

token_t *parse_param_expand(parser_t *parser, token_t *token)
{
    CHECK_PARSER();
    PARSER_ASSERT(token->type == TOKEN_DOLLAR || token->type == TOKEN_DOLLAR_LBRACE);

    if (token->type == TOKEN_DOLLAR) {
        token_t *var_name = get_name(parser, false);

        if (var_name != NULL) {
            PARSER_PUSH_ILs(IL_PUSH_NAME, var_name->payload);
            PARSER_PUSH_IL(IL_EXPAND_PARAM);
        } else {
            parser->last_error = PARSER_ERR_UNEXPECTED;
        }

        free(var_name);
        return NULL;
    }

    token_t *var_name = get_name(parser, true);
    PARSER_PUSH_ILs(IL_PUSH_NAME, var_name->payload);
    PARSER_PUSH_IL(IL_EXPAND_PARAM);
    free(var_name);

    int peek = peek_char(parser);
    switch (peek)
    {
    case ':': case '-': case '=': case '?': case '+':
        parser->last_error = PARSER_ERR_NOT_IMPLEMENTED;
        return NULL;
    case '}':
        get_char(parser);
        return NULL;
    case EOF:
        parser->last_error = PARSER_ERR_INCOMPLETE;
        return NULL;
    default:
        parser->last_error = PARSER_ERR_UNEXPECTED;
        return NULL;
    }
}

token_t *parse_subcmd_expand(parser_t *parser, token_t *token)
{
    CHECK_PARSER();
    UNUSED_VAR(token);
    parser->last_error = PARSER_ERR_NOT_IMPLEMENTED;
    return NULL;
}

token_t *parse_word(parser_t *parser, token_t *token)
{
    CHECK_PARSER();

    token_t *partial = token;
    bool keep_parsing = true;

    PARSER_PUSH_IL(IL_PUSH_WORDINIT);

    while (keep_parsing) {
        if (partial->type == TOKEN_DOLLAR || partial->type == TOKEN_DOLLAR_LBRACE) {
            token_t *peek = parse_param_expand(parser, partial);
            if (peek != NULL) free(peek);
            PARSER_ASSERT(peek == NULL);
        } else if (partial->type == TOKEN_PARTIAL_WORD || partial->type == TOKEN_PARTIAL_ASSIGN_WORD) {
            PARSER_PUSH_ILs(IL_PUSH_PARTIAL, partial->payload);
        } else if (partial->type == TOKEN_WORD_END) {
            PARSER_PUSH_ILs(IL_PUSH_PARTIAL, partial->payload);
            PARSER_PUSH_IL(IL_COMPOSE_WORD);
            if (token->type == TOKEN_PARTIAL_ASSIGN_WORD) {
                PARSER_PUSH_IL(IL_ASSIGN_WORD);
            }
            keep_parsing = false;
        } else if (partial->type == TOKEN_ASSIGN_WORD_END) {
            PARSER_PUSH_ILs(IL_PUSH_PARTIAL, partial->payload);
            PARSER_PUSH_IL(IL_COMPOSE_WORD);
            PARSER_PUSH_IL(IL_ASSIGN_WORD);
            keep_parsing = false;
        } else {
            if (partial != token) free(partial);
            parser->last_error = PARSER_ERR_UNEXPECTED;
            return NULL;
        }

        if (partial != token) free(partial);
        if (keep_parsing) partial = get_token(parser, LEX_HINT_COMPOSING_WORD);
    }

    return NULL;
}

token_t *parse_io_redir(parser_t *parser, token_t *token)
{
    CHECK_PARSER();
    PARSER_ASSERT(token->type == TOKEN_IO_NUMBER
               || token->type == TOKEN_LESS
               || token->type == TOKEN_DLESS
               || token->type == TOKEN_GREAT
               || token->type == TOKEN_DGREAT
               || token->type == TOKEN_LESSGREAT
               || token->type == TOKEN_LESSAND
               || token->type == TOKEN_GREATAND
               || token->type == TOKEN_DLESSDASH
               || token->type == TOKEN_CLOBBER);

    int fd = -1;
    io_redir_type_t redir_type = IO_REDIR_UNKNOWN;
    token_t *redir_op = token;

    if (token->type == TOKEN_IO_NUMBER) {
        PARSER_ASSERT(sscanf(token->payload, "%d", &fd) == 1);
        PARSER_ASSERT(fd >= 0);
        redir_op = get_token(parser, LEX_NO_HINT);
    }

    if (redir_op->payload[0] == '<') {
        fd = fd < 0 ? 0 : fd;
    } else {
        fd = fd < 0 ? 1 : fd;
    }

    switch (redir_op->type) {
    case TOKEN_LESS:      redir_type = IO_REDIR_INPUT; break;
    case TOKEN_DLESS:     redir_type = IO_REDIR_HEREDOC; break;
    case TOKEN_DLESSDASH: redir_type = IO_REDIR_HEREDOC; break;
    case TOKEN_GREAT:     redir_type = IO_REDIR_OUTPUT; break;
    case TOKEN_DGREAT:    redir_type = IO_REDIR_OUTPUT_APPEND; break;
    case TOKEN_LESSGREAT: redir_type = IO_REDIR_INOUT; break;
    case TOKEN_LESSAND:   redir_type = IO_REDIR_INPUT_DUP; break;
    case TOKEN_GREATAND:  redir_type = IO_REDIR_OUTPUT_DUP; break;
    case TOKEN_CLOBBER:   redir_type = IO_REDIR_OUTPUT_CLOBBER; break;
    default:
        parser->last_error = PARSER_ERR_UNEXPECTED;
        if (token->type == TOKEN_IO_NUMBER) free(redir_op);
        return NULL;
    }

    if (redir_op != token) free(redir_op);

    if (redir_type != IO_REDIR_INPUT_DUP && redir_type != IO_REDIR_OUTPUT_DUP) {
        token_t *peek = get_token(parser, LEX_NO_HINT);
        token_t *subpeek = parse_word(parser, peek);
        if (subpeek != NULL) {
            free(subpeek);
            parser->last_error = PARSER_ERR_UNEXPECTED;
        }
        free(peek);
        if (subpeek != NULL) return NULL;
    } else {
        token_t *peek = get_io_number(parser);
        int fd_dup = -1;
        if (peek != NULL) {
            if(!(sscanf(peek->payload, "%d", &fd_dup) == 1) || fd_dup < 0) {
                free(peek);
                parser->last_error = PARSER_ERR_UNEXPECTED;
                return NULL;
            }
            free(peek);
            PARSER_PUSH_ILi(IL_PUSH_FD, fd_dup);
        }
    }

    if (redir_type == IO_REDIR_HEREDOC) {
        parser->last_error = PARSER_ERR_NOT_IMPLEMENTED;
        return NULL;
    }

    PARSER_PUSH_ILi(IL_PUSH_FD, fd);
    PARSER_PUSH_ILi(IL_PUSH_REDIR, (int)redir_type);
    PARSER_PUSH_IL(IL_COMPOSE_IOREDIR);

    return NULL;
}

static bool is_simple_cmd_part(token_t *token)
{
    switch (token->type) {
    case TOKEN_PARTIAL_WORD:
    case TOKEN_WORD_END:
    case TOKEN_PARTIAL_ASSIGN_WORD:
    case TOKEN_ASSIGN_WORD_END:
    case TOKEN_IO_NUMBER:
    case TOKEN_LESS:
    case TOKEN_DLESS:
    case TOKEN_DLESSDASH:
    case TOKEN_GREAT:
    case TOKEN_DGREAT:
    case TOKEN_LESSGREAT:
    case TOKEN_LESSAND:
    case TOKEN_GREATAND:
    case TOKEN_CLOBBER:
    case TOKEN_DOLLAR:
    case TOKEN_DOLLAR_LBRACE:
    case TOKEN_DOLLAR_LPAREN:
        return true;
    default:
        return false;
    }
}

token_t *parse_simple_cmd(parser_t *parser, token_t *token)
{
    lex_hint_t lex_hint = LEX_HINT_CMD_PREFIX;

    token_t *peek = token;
    if (!is_simple_cmd_part(peek)) PARSER_THROW(PARSER_ERR_UNEXPECTED);

    PARSER_PUSH_IL(IL_PUSH_CMDINIT);

    while (is_simple_cmd_part(peek)) {
        switch (peek->type) {
        case TOKEN_PARTIAL_WORD:
        case TOKEN_WORD_END:
        case TOKEN_DOLLAR:
        case TOKEN_DOLLAR_LBRACE:
        case TOKEN_DOLLAR_LPAREN:
            lex_hint = LEX_HINT_CMD_POSTFIX;
            EXPLICIT_FALLTHROUGH;
        case TOKEN_PARTIAL_ASSIGN_WORD:  // Qt Creator's parser sucks
        case TOKEN_ASSIGN_WORD_END: {
            PARSER_ASSERT_NULL(parse_word(parser, peek));
            break;
        }
        case TOKEN_IO_NUMBER:
        case TOKEN_LESS:
        case TOKEN_DLESS:
        case TOKEN_DLESSDASH:
        case TOKEN_GREAT:
        case TOKEN_DGREAT:
        case TOKEN_LESSGREAT:
        case TOKEN_LESSAND:
        case TOKEN_GREATAND:
        case TOKEN_CLOBBER: {
            PARSER_ASSERT_NULL(parse_io_redir(parser, peek));
            break;
        }
        default:
            PARSER_THROW(PARSER_ERR_UNEXPECTED);
        }

        PARSER_NEXT(lex_hint);
    }

    PARSER_PUSH_IL(IL_COMPOSE_COMMAND);

    PARSER_RETURN();
}

#define PARSER_EXEC(x) \
    do { \
        token_t *subpeek = (x); \
        if (parser->last_error != PARSER_NO_ERROR) { \
            if (subpeek != NULL && subpeek != token) free(subpeek); \
            if (peek != token) free(peek); \
            return NULL; \
        } \
        if (subpeek == NULL) subpeek = get_token(parser, LEX_HINT_CMD_PREFIX_KW); \
        if (peek != token) free(peek); \
        peek = subpeek; \
    } while (0)

#define PARSER_MATCH(x) \
    do { \
        if (peek->type != (x)) { \
            parser->last_error = PARSER_ERR_UNEXPECTED; \
            if (peek != token) free(peek); \
            return NULL; \
        } \
        if (peek != token) free(peek); \
        peek = get_token(parser, LEX_HINT_CMD_PREFIX_KW); \
    } while (0)

static bool is_keyword(token_t *token)
{
    switch (token->type) {
    case TOKEN_FOR:
    case TOKEN_IN:
    case TOKEN_DO:
    case TOKEN_DONE:
    case TOKEN_CASE:
    case TOKEN_ESAC:
    case TOKEN_WHILE:
    case TOKEN_UNTIL:
    case TOKEN_SELECT:
    case TOKEN_IF:
    case TOKEN_ELSE:
    case TOKEN_ELIF:
    case TOKEN_FI:
        return true;
    default:
        return false;
    }
}

token_t *parse_command(parser_t *parser, token_t *token)
{
    token_t *peek = token;

    if (is_keyword(token)) {
        PARSER_THROW(PARSER_ERR_NOT_IMPLEMENTED);
    }

    PARSER_EXEC(parse_simple_cmd(parser, peek));
    PARSER_RETURN();
}

token_t *parse_newlines(parser_t *parser, token_t *token)
{
    CHECK_PARSER();

    token_t *peek = token;

    while (peek->type == TOKEN_NEWLINE) {
        PARSER_MATCH(TOKEN_NEWLINE);
    }

    PARSER_RETURN();
}

token_t *parse_pipeline(parser_t *parser, token_t *token)
{
    CHECK_PARSER();

    token_t *peek = token;
    if (peek->type == TOKEN_BANG) {
        peek = get_token(parser, LEX_HINT_CMD_PREFIX_KW);
    }

    PARSER_EXEC(parse_command(parser, peek));

    while(peek->type == TOKEN_BAR) {
        PARSER_MATCH(TOKEN_BAR);
        PARSER_ASSERT_NOT_EOF();
        if (peek->type == TOKEN_NEWLINE) {
            PARSER_EXEC(parse_newlines(parser, peek));
        }
        PARSER_EXEC(parse_command(parser, peek));
        PARSER_PUSH_IL(IL_PIPELINE_LINK);
    }

    if (token->type == TOKEN_BANG) {
        PARSER_PUSH_IL(IL_PENDING_NOT);
    }

    PARSER_RETURN();
}

static bool is_pipeline_first(token_t *token)
{
    if (is_simple_cmd_part(token)) return true;
    if (token->type == TOKEN_BAR) return true;
    return false;
}

static bool is_list_first(token_t *token)
{
    if (is_pipeline_first(token)) return true;
    switch (token->type) {
    case TOKEN_SEMI:
    case TOKEN_AMP:
        return true;
    default:
        return false;
    }
}

token_t *parse_list(parser_t *parser, token_t *token)
{
    CHECK_PARSER();

    token_t *peek = token;

    while (is_list_first(peek)) {
        bool has_command = is_pipeline_first(peek);
        if (has_command) {
            PARSER_EXEC(parse_pipeline(parser, peek));
        }

        switch (peek->type) {
        case TOKEN_EOF:
            if (has_command) PARSER_PUSH_IL(IL_EXEC_PIPELINE);
            break;
        case TOKEN_SEMI:
            if (has_command) PARSER_PUSH_IL(IL_EXEC_PIPELINE);
            PARSER_MATCH(TOKEN_SEMI);
            if (peek->type == TOKEN_NEWLINE) {
                PARSER_EXEC(parse_newlines(parser, peek));
            }
            break;
        case TOKEN_AMP:
            if (!has_command) {
                PARSER_THROW(PARSER_ERR_UNEXPECTED);
            }
            PARSER_PUSH_IL(IL_EXEC_BACKGROUND);
            PARSER_MATCH(TOKEN_AMP);
            if (peek->type == TOKEN_NEWLINE) {
                PARSER_EXEC(parse_newlines(parser, peek));
            }
            break;
        case TOKEN_NEWLINE:
            PARSER_PUSH_IL(IL_EXEC_PIPELINE);
            PARSER_EXEC(parse_newlines(parser, peek));
            break;
        default:
            PARSER_THROW(PARSER_ERR_UNEXPECTED);
        }
    }

    PARSER_RETURN();
}
