#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <readline/readline.h>
#include "reader.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "alias.h"
#include "vm.h"
#include "states.h"

//#include "il_t.inc.h"
//vm_error_t vm_exec1(vm_t *vm, il_t *il);

//int main()
//{
//    vm_t *vm = vm_new();
//    if (vm == NULL) panic("vm alloc error");
//    il_list_t ils; ils.array = NULL; ils.capacity = 0; ils.size = 0;
//    if (!il_list_init(&ils)) panic("il init error");
//    il_list_push(&ils, IL_PUSH_CMDINIT);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "ls");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_COMPOSE_COMMAND);
//    il_list_push(&ils, IL_PUSH_CMDINIT);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "grep");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "-i");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "a");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_COMPOSE_COMMAND);
//    il_list_push(&ils, IL_PIPELINE_LINK);
//    il_list_push(&ils, IL_PUSH_CMDINIT);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "FOO=BAR");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_ASSIGN_WORD);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "lorem");
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "ipsum");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_NAME, "HOME");
//    il_list_push(&ils, IL_EXPAND_PARAM);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "foo");
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "bar");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_push(&ils, IL_PUSH_WORDINIT);
//    il_list_pushs(&ils, IL_PUSH_PARTIAL, "lorem");
//    il_list_push(&ils, IL_COMPOSE_WORD);
//    il_list_pushi(&ils, IL_PUSH_FD, 1);
//    il_list_pushi(&ils, IL_PUSH_REDIR, IO_REDIR_OUTPUT);
//    il_list_push(&ils, IL_COMPOSE_IOREDIR);
//    il_list_pushi(&ils, IL_PUSH_FD, 1);
//    il_list_pushi(&ils, IL_PUSH_FD, 2);
//    il_list_pushi(&ils, IL_PUSH_REDIR, IO_REDIR_OUTPUT_DUP);
//    il_list_push(&ils, IL_COMPOSE_IOREDIR);
//    il_list_push(&ils, IL_COMPOSE_COMMAND);
//    il_list_push(&ils, IL_PIPELINE_LINK);

//    vm_exec(vm, &ils);

//    il_list_free(&ils);
//    vm_free(vm);
//    return 0;
//}

int main()
{
    init_env();
    alias_init();
    reader_set_histsize(NULL);
    reader_load_history();
    char *line = NULL;
    vm_t *vm = vm_new();
    rl_attempted_completion_function = &reader_completion;
    rl_completer_quote_characters = "'";

    while (true) {
        char *input = (line == NULL) ? reader_readline() : reader_readmore();
        if (line == NULL) {
            if (input == NULL) break; else line = input;
        } else if (input != NULL) {
            char *new_line = str_join(line, "\n", input);
            free(line);
            line = new_line;
        }
        line = reader_expand_history(line);
        if (state_debug) {
            printf("Your input: ");
            print_str_repr(line, -1);
            putchar('\n');
        }

        parser_t *parser = parser_new(line, line + strlen(line));
        token_t *peek = get_token(parser, LEX_HINT_CMD_PREFIX_KW);
        if (peek->type == TOKEN_EOF) {
            free(peek);
            parser_free(parser);
            free(line);
            line = NULL;
            continue;
        }
        if (state_debug) {
            printf("Initial Peek: %s(", token_type_name(peek->type));
            print_str_repr(peek->payload, -1);
            printf(")\n");
        }
        token_t *subpeek = parse_list(parser, peek);
        if (subpeek != NULL) free(subpeek);
        if (state_debug) parser_dump(parser);
        if (parser_error(parser) == PARSER_ERR_INCOMPLETE && input != NULL) {
            if (state_debug) il_list_dump(parser_il_list(parser));
            free(peek);
            parser_free(parser);
            continue;
        } else if (parser_error(parser) == PARSER_NO_ERROR) {
            vm_clear(vm);
            vm_exec(vm, parser_il_list(parser));
        } else {
            printf("nsh: parser: %s\n", parser_strerror(parser));
        }

        reader_addhist(line);
        free(peek);
        parser_free(parser);
        free(line);
        line = NULL;
    }
    vm_free(vm);
    reader_save_history();
    return 0;
}
