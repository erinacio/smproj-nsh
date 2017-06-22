#ifndef LEXER_H
#define LEXER_H

typedef enum lex_hint_e {
    LEX_NO_HINT,
    LEX_HINT_COMPOSING_WORD,  // will generate TOKEN_WORD_END when meet a blank
    LEX_HINT_CMD_PREFIX,      // will detect for io-redir and assign-word
    LEX_HINT_CMD_PREFIX_KW,   // also detect for keywords
    LEX_HINT_EXPECT_IN,       // 'in' and only 'in' will be considered as KW
    LEX_HINT_CMD_POSTFIX,     // will detect for io-redir
} lex_hint_t;

static const bool LNAME_IN_BRACE = true;
static const bool LNAME_NO_BRACE = false;

typedef enum token_type_e {
    TOKEN_INVALID,
    TOKEN_EOF,
    TOKEN_NEWLINE,

    TOKEN_PARTIAL_WORD,
    TOKEN_WORD_END,
    TOKEN_PARTIAL_ASSIGN_WORD,
    TOKEN_ASSIGN_WORD_END,
    TOKEN_NAME,
    TOKEN_IO_NUMBER,

    TOKEN_LESS,       // <
    TOKEN_DLESS,      // <<
    TOKEN_GREAT,      // >
    TOKEN_DGREAT,     // >>
    TOKEN_LESSAND,    // <&
    TOKEN_GREATAND,   // >&
    TOKEN_DLESSDASH,  // <<-
    TOKEN_LESSGREAT,  // <>
    TOKEN_CLOBBER,    // >|
    TOKEN_SEMI,       // ;
    TOKEN_DSEMI,      // ;;
    TOKEN_LBRACE,     // {
    TOKEN_RBRACE,     // }
    TOKEN_DOLLAR,     // $
    TOKEN_DOLLAR_LBRACE,  // ${
    TOKEN_DOLLAR_LPAREN,  // $(
    TOKEN_LPAREN,     // (
    TOKEN_RPAREN,     // )
    TOKEN_AMP,        // &
    TOKEN_DAMP,       // &&
    TOKEN_BAR,        // |
    TOKEN_DBAR,       // ||
    TOKEN_BANG,       // !

    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_DO,
    TOKEN_DONE,
    TOKEN_CASE,
    TOKEN_ESAC,
    TOKEN_WHILE,
    TOKEN_UNTIL,
    TOKEN_SELECT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_ELIF,
    TOKEN_FI,

    TOKENS_COUNT,
} token_type_t;

typedef struct parser_s parser_t;

typedef struct token_s {
    token_type_t type;
    char payload[];
} token_t;

const char *token_type_name(token_type_t type);
int peek_char(parser_t *parser);
int get_char(parser_t *parser);
token_t *get_token(parser_t *parser, lex_hint_t hint);
token_t *get_name(parser_t *parser, bool in_brace);
token_t *get_io_number(parser_t *parser);

#endif // LEXER_H
