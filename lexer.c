#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "parser_t.inc.h"
#include "alias.h"
#include "states.h"

#define AEOF -2 // alias EOF

const char *token_type_name(token_type_t type)
{
#define _MKENT(x) case x: return #x
    switch (type) {
    _MKENT(TOKEN_INVALID);
    _MKENT(TOKEN_EOF);
    _MKENT(TOKEN_NEWLINE);
    _MKENT(TOKEN_PARTIAL_WORD);
    _MKENT(TOKEN_WORD_END);
    _MKENT(TOKEN_NAME);
    _MKENT(TOKEN_PARTIAL_ASSIGN_WORD);
    _MKENT(TOKEN_ASSIGN_WORD_END);
    _MKENT(TOKEN_IO_NUMBER);
    _MKENT(TOKEN_LESS);
    _MKENT(TOKEN_DLESS);
    _MKENT(TOKEN_GREAT);
    _MKENT(TOKEN_DGREAT);
    _MKENT(TOKEN_LESSAND);
    _MKENT(TOKEN_GREATAND);
    _MKENT(TOKEN_DLESSDASH);
    _MKENT(TOKEN_LESSGREAT);
    _MKENT(TOKEN_CLOBBER);
    _MKENT(TOKEN_SEMI);
    _MKENT(TOKEN_DSEMI);
    _MKENT(TOKEN_LBRACE);
    _MKENT(TOKEN_RBRACE);
    _MKENT(TOKEN_DOLLAR);
    _MKENT(TOKEN_DOLLAR_LBRACE);
    _MKENT(TOKEN_DOLLAR_LPAREN);
    _MKENT(TOKEN_LPAREN);
    _MKENT(TOKEN_RPAREN);
    _MKENT(TOKEN_AMP);
    _MKENT(TOKEN_DAMP);
    _MKENT(TOKEN_BAR);
    _MKENT(TOKEN_DBAR);
    _MKENT(TOKEN_BANG);
    _MKENT(TOKEN_FOR);
    _MKENT(TOKEN_IN);
    _MKENT(TOKEN_DO);
    _MKENT(TOKEN_DONE);
    _MKENT(TOKEN_CASE);
    _MKENT(TOKEN_ESAC);
    _MKENT(TOKEN_WHILE);
    _MKENT(TOKEN_UNTIL);
    _MKENT(TOKEN_SELECT);
    _MKENT(TOKEN_IF);
    _MKENT(TOKEN_ELSE);
    _MKENT(TOKEN_ELIF);
    _MKENT(TOKEN_FI);
    default: return "TOKEN_UNKNOWN";
    }
#undef _MKENT
}

#define peek_char_return_eof() \
    do { \
        parser->peek = EOF; \
        parser->peek_len = 0; \
        return parser->peek; \
    } while (0)

#define peek_char_return_aeof() \
    do { \
        al->peek = AEOF; \
        al->peek_len = 0; \
        return al->peek; \
    } while (0)

int peek_char_noalias(parser_t *parser)
{
    // TODO: report error if we read NUL in the input
    if (parser->peek != '\0') return parser->peek;
    if (parser->curr == parser->input_end) peek_char_return_eof();
    char *curr = parser->curr;
    while (*curr++ == '\\') {
        if (curr == parser->input_end) break;  // EOF after backslash
        if (*curr != '\n') break;
        if (++curr == parser->input_end) peek_char_return_eof();  // EOF after line concatenation
    }
    parser->peek = ((uint8_t *)curr)[-1];
    parser->peek_len = curr - parser->curr;
    return parser->peek;
}

int peek_char(parser_t *parser)
{
    if (parser->alias_lexer == NULL) return peek_char_noalias(parser);
    alias_lexer_t *al = parser->alias_lexer;
    while (al->alias_lexer != NULL) al = al->alias_lexer;
    if (al->peek != '\0') return al->peek;

    // TODO: use macro to merge these code with almost the same code in peek_char_noalias
    if (al->curr == al->input_end) peek_char_return_aeof();
    char *curr = al->curr;
    while (*curr++ == '\\') {
        if (curr == al->input_end) break;
        if (*curr != '\n') break;
        if (++curr == al->input_end) peek_char_return_aeof();
    }
    al->peek = ((uint8_t *)curr)[-1];
    al->peek_len = curr - al->curr;
    return al->peek;
}

int get_char_noalias(parser_t *parser)
{
    if (parser->peek == '\0') peek_char_noalias(parser);
    int ch = parser->peek;
    parser->curr += parser->peek_len;
    parser->peek = '\0';
    parser->peek_len = 0;
    return ch;
}

int get_char(parser_t *parser) {
    if (parser->alias_lexer == NULL) return get_char_noalias(parser);
    alias_lexer_t *al = parser->alias_lexer;
    while (al->alias_lexer != NULL) al = al->alias_lexer;
    if (al->peek == '\0') peek_char(parser);
    int ch = al->peek;
    al->curr += al->peek_len;
    if (ch == AEOF) {
        alias_lexer_t **pal = &parser->alias_lexer;
        while ((*pal)->alias_lexer != NULL) pal = &(*pal)->alias_lexer;
        free(*pal);
        *pal = NULL;
    } else {
        al->peek = '\0';
        al->peek_len = 0;
    }
    return ch;
}

static bool is_op2_init(int ch)
{
    return ch == '<' || ch == '>' || ch == ';'
        || ch == '$' || ch == '&' || ch == '|';
}

static bool must_be_op1(int ch)
{
    return ch == '{' || ch == '}' ||  ch == '(' || ch == ')' || ch == '!';
}

static bool is_op_init(int ch)
{
    return must_be_op1(ch) || is_op2_init(ch);
}

static bool can_compose_op2(int ch1, int ch2)
{
    switch (ch1)
    {
    case '<': return ch2 == '<' || ch2 == '>' || ch2 == '&';
    case '>': return ch2 == '>' || ch2 == '|' || ch2 == '&';
    case ';': return ch2 == ';';
    case '$': return ch2 == '(' || ch2 == '{';
    case '&': return ch2 == '&';
    case '|': return ch2 == '|';
    default:
        return false;
    }
}

static bool wont_be_word(int ch)
{
    return (is_op_init(ch) && ch != '$') || ch == EOF || ch == AEOF || ch == '#'
            || (ch < 0x80 && !isgraph(ch));
}

static token_type_t get_op1_type(int ch)
{
    switch (ch) {
    case '<': return TOKEN_LESS;
    case '>': return TOKEN_GREAT;
    case ';': return TOKEN_SEMI;
    case '{': return TOKEN_LBRACE;
    case '}': return TOKEN_RBRACE;
    case '$': return TOKEN_DOLLAR;
    case '(': return TOKEN_LPAREN;
    case ')': return TOKEN_RPAREN;
    case '&': return TOKEN_AMP;
    case '|': return TOKEN_BAR;
    case '!': return TOKEN_BANG;
    default:  return TOKEN_INVALID;
    }
}

static token_type_t get_op2_type(int ch1, int ch2)
{
    switch (ch1)
    {
    case '<':
        if (ch2 == '<') return TOKEN_DLESS;
        if (ch2 == '>') return TOKEN_LESSGREAT;
        if (ch2 == '&') return TOKEN_LESSAND;
        return TOKEN_INVALID;
    case '>':
        if (ch2 == '>') return TOKEN_DGREAT;
        if (ch2 == '|') return TOKEN_CLOBBER;
        if (ch2 == '&') return TOKEN_GREATAND;
        return TOKEN_INVALID;
    case ';':
        return ch2 == ';' ? TOKEN_DSEMI : TOKEN_INVALID;
    case '$':
        if (ch2 == '(') return TOKEN_DOLLAR_LPAREN;
        if (ch2 == '{') return TOKEN_DOLLAR_LBRACE;
        return TOKEN_INVALID;
    case '&':
        return ch2 == '&' ? TOKEN_DAMP : TOKEN_INVALID;
    case '|':
        return ch2 == '|' ? TOKEN_DBAR : TOKEN_INVALID;
    default:
        return TOKEN_INVALID;
    }
}

static void skip_blanks(parser_t *parser)
{
    while (isblank(peek_char(parser))) get_char(parser);
}

static void skip_till_nl(parser_t *parser)
{
    while (get_char(parser) != '\n') continue;
}

static void skip_unimportant(parser_t *parser)
{
    while (true) {
        int peek = peek_char(parser);
        if (isblank(peek)) {
            skip_blanks(parser);
        } else if (peek == '#') {
            if (peek_char(parser) == '#') {
                skip_till_nl(parser);
                skip_blanks(parser);
            }
        } else if (peek == AEOF) {
            get_char(parser);
        } else {
            return;
        }
    }
}

static token_t *make_token(token_type_t type, const char *begin, const char *end)
{
    ptrdiff_t token_len = end - begin;
    if (token_len < 0) return NULL;

    token_t *token = malloc(sizeof(token_t) + token_len + 1);
    if (token == NULL) return NULL;

    memcpy(token->payload, begin, token_len);
    token->payload[token_len] = '\0';
    token->type = type;

    return token;
}

static void expect_keyword(const token_t *token, token_type_t *expected, const char *the_keyword, token_type_t when_match)
{
    if (strcmp(token->payload, the_keyword) == 0) {
        *expected =  when_match;
    }
}

static void expect_io_number(parser_t *parser, const token_t *token, token_type_t *expected)
{
    if (*expected != TOKEN_WORD_END) return;

    const char *ch = token->payload;
    while (*ch != '\0') {
        if (!isdigit(*ch)) return;
        ++ch;
    }
    if (*ch != '\0') return;

    int peek = peek_char(parser);
    if (peek == '<' || peek == '>') *expected = TOKEN_IO_NUMBER;
}

static bool is_var_part(int ch)
{
    return isalnum(ch) || ch == '_';
}

static void expect_assign_word(const token_t *token, token_type_t *expected)
{
    if (*expected != TOKEN_WORD_END && *expected != TOKEN_PARTIAL_WORD) return;

    const char *ch = token->payload;
    if (!isalpha(*ch) && *ch != '_') return;
    while (*ch != '\0') {
        if (!is_var_part(*ch)) break;
        ++ch;
    }
    if (*ch == '=') *expected = (*expected == TOKEN_WORD_END) ? TOKEN_ASSIGN_WORD_END : TOKEN_PARTIAL_ASSIGN_WORD;
}

static bool token_push_alias(parser_t *parser, const char *alias_name)
{
    for (alias_lexer_t *al = parser->alias_lexer; al != NULL; al = al->alias_lexer) {
        if (strcmp(al->alias_name, alias_name) == 0) return false;
    }
    if (!alias_in(alias_name)) return false;
    const char *val = alias_get(alias_name);
    if (val == NULL) return false;
    // FIXME: check for overflow
    alias_lexer_t *al = malloc(sizeof(alias_lexer_t) + strlen(val) + 1);
    if (al == NULL) return false;
    strcpy(al->input, val);
    al->alias_name = strdup(alias_name);
    al->curr = al->input;
    al->input_end = al->input + strlen(al->input);
    al->alias_lexer = NULL;
    al->peek = '\0';
    al->peek_len = 0;

    alias_lexer_t **pal = &parser->alias_lexer;
    while (*pal != NULL) pal = &(*pal)->alias_lexer;
    *pal = al;

    return true;
}

static token_type_t token_type_hinting(parser_t *parser, const token_t *token, lex_hint_t hint)
{
    token_type_t type = token->type;
    if (type != TOKEN_PARTIAL_WORD) return type;

    switch (hint) {
    case LEX_HINT_COMPOSING_WORD:
        if (wont_be_word(peek_char(parser))) type = TOKEN_WORD_END;
        return type;
    case LEX_HINT_CMD_PREFIX: case LEX_HINT_CMD_PREFIX_KW:
        if (wont_be_word(peek_char(parser))) type = TOKEN_WORD_END;
        expect_assign_word(token, &type);
        if (!wont_be_word(peek_char(parser))) return type;
        if (hint == LEX_HINT_CMD_PREFIX_KW) {
            expect_keyword(token, &type, "for",    TOKEN_FOR);
            expect_keyword(token, &type, "do",     TOKEN_DO);
            expect_keyword(token, &type, "done",   TOKEN_DONE);
            expect_keyword(token, &type, "case",   TOKEN_CASE);
            expect_keyword(token, &type, "esac",   TOKEN_ESAC);
            expect_keyword(token, &type, "while",  TOKEN_WHILE);
            expect_keyword(token, &type, "until",  TOKEN_UNTIL);
            expect_keyword(token, &type, "select", TOKEN_SELECT);
            expect_keyword(token, &type, "if",     TOKEN_IF);
            expect_keyword(token, &type, "else",   TOKEN_ELSE);
            expect_keyword(token, &type, "elif",   TOKEN_ELIF);
            expect_keyword(token, &type, "fi",     TOKEN_FI);
        }
        expect_io_number(parser, token, &type);
        return type;
    case LEX_HINT_CMD_POSTFIX:
        if (wont_be_word(peek_char(parser))) type = TOKEN_WORD_END;
        expect_io_number(parser, token, &type);
        return type;
    case LEX_HINT_EXPECT_IN:
        expect_keyword(token, &type, "in", TOKEN_IN);
        return type;
    default:
        return type;
    }
}

static char *get_parser_curr(parser_t *parser)
{
    if (parser->alias_lexer == NULL) {
         return parser->curr;
    } else {
        alias_lexer_t *al = parser->alias_lexer;
        while (al->alias_lexer != NULL) al = al->alias_lexer;
        return al->curr;
    }
}

#define RETURN_TOKEN(type) \
    do { \
        return make_token((type), token_begin, get_parser_curr(parser)); \
    } while (0)

#define RETURN_OP1(ch)      RETURN_TOKEN(get_op1_type((ch)))
#define RETURN_OP2(a, b)    RETURN_TOKEN(get_op2_type((a), (b)))

static token_t *get_token_(parser_t *parser, lex_hint_t hint)
{
    if (hint != LEX_HINT_COMPOSING_WORD) {
        skip_unimportant(parser);
    }

    char *token_begin = get_parser_curr(parser);
    if (hint == LEX_HINT_COMPOSING_WORD && wont_be_word(peek_char(parser))) {
        RETURN_TOKEN(TOKEN_WORD_END);
    }

    int ch = get_char(parser);

    // new line
    if (ch == '\n') {
        RETURN_TOKEN(TOKEN_NEWLINE);

    // 1ch operator
    } else if (is_op_init(ch) && !can_compose_op2(ch, peek_char(parser))) {
        RETURN_OP1(ch);

    // 2ch / 3ch operator
    } else if (can_compose_op2(ch, peek_char(parser))) {
        int ch2 = get_char(parser);
        if (peek_char(parser) != '-') {
            RETURN_OP2(ch, ch2);
        } else {
            get_char(parser);
            RETURN_TOKEN(TOKEN_DLESSDASH);
        }

    // EOF
    } else if (ch == EOF) {
        RETURN_TOKEN(TOKEN_EOF);

    // unexpected char
    } else if (!isprint(ch)) {
        RETURN_TOKEN(TOKEN_INVALID);
    }

    // words / paths / keywords / names
    bool single_quote = false;
    bool backslash = false;
    if (ch == '\'') single_quote = !single_quote;
    if (ch == '\\') backslash = true;

    while (true) {
        int peek = peek_char(parser);

        // Handle unexpected EOF
        if (peek == EOF && (single_quote || backslash)) {
            parser->last_error = PARSER_ERR_INCOMPLETE;
            RETURN_TOKEN(TOKEN_INVALID);
        }

        // Handle unexpected AEOF
        if (peek == AEOF && (single_quote || backslash)) {
            parser->last_error = PARSER_ERR_UNEXPECTED;
            RETURN_TOKEN(TOKEN_INVALID);
        }

        // Handle normal word termination
        if ((wont_be_word(peek) || peek == '$') && !(single_quote || backslash)) {
            token_t *token = make_token(TOKEN_PARTIAL_WORD, token_begin, get_parser_curr(parser));
            token->type = token_type_hinting(parser, token, hint);
            if (hint != LEX_HINT_CMD_PREFIX && hint != LEX_HINT_CMD_PREFIX_KW) {
                return token;
            }
            if (token->type == TOKEN_WORD_END) {
                if (!token_push_alias(parser, token->payload)) return token;
                token_t *new_token = get_token(parser, LEX_HINT_CMD_PREFIX);
                if (new_token == NULL) return token;
                free(token);
                return new_token;
            }
            return token;
        }

        // Handle single quote
        if (peek == '\'' && (single_quote || !backslash)) {
            single_quote = !single_quote;
        }

        // Handle backslash
        if (peek == '\\' && single_quote == false && backslash == false) {
            backslash = true;
        } else {
            backslash = false;
        }

        ch = get_char(parser);
    }

    panic("get_token entered unknown state.");
    return NULL;
}

token_t *get_token(parser_t *parser, lex_hint_t hint)
{
    token_t *token = get_token_(parser, hint);
    if (state_debug) {
        if (token == NULL) {
            printf("TOKEN_(null)\n");
        } else {
            printf("%s: ", token_type_name(token->type));
            print_str_repr(token->payload, -1);
            putchar('\n');
        }
    }
    return token;
}

static bool is_special_param(int ch)
{
    return ch == '@' || ch == '*' || ch == '#' || ch == '?' || ch == '-'
        || ch == '$' || ch == '!';
}

token_t *get_name(parser_t *parser, bool in_brace)
{
    skip_unimportant(parser);

    char *token_begin = parser->curr;

    int peek = peek_char(parser);
    if (!(is_var_part(peek) || is_special_param(peek))) return NULL;

    int ch = get_char(parser);
    if (is_special_param(ch)) RETURN_TOKEN(TOKEN_NAME);
    if (isdigit(ch) && !in_brace) RETURN_TOKEN(TOKEN_NAME);

    bool num_only = isdigit(peek);
    while (true) {
        peek = peek_char(parser);
        if (!is_var_part(peek)) RETURN_TOKEN(TOKEN_NAME);
        if (num_only && !isdigit(peek)) RETURN_TOKEN(TOKEN_NAME);

        ch = get_char(parser);
    }

    panic("get_name entered unknown state.");
    return NULL;
}

token_t *get_io_number(parser_t *parser)
{
    skip_unimportant(parser);

    char *token_begin = parser->curr;
    int peek = peek_char(parser);
    if (!isdigit(peek)) {
        parser->last_error = PARSER_ERR_UNEXPECTED;
        return NULL;
    }

    while (isdigit(peek_char(parser))) get_char(parser);

    RETURN_TOKEN(TOKEN_IO_NUMBER);
}
