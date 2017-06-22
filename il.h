#ifndef IL_H
#define IL_H

#include <stdbool.h>

typedef enum io_redir_type_e {
    IO_REDIR_UNKNOWN,     // used as initial value only
    IO_REDIR_INPUT,       // '<', TOKEN_LESS
    IO_REDIR_OUTPUT,      // '>', TOKEN_GREAT
    IO_REDIR_OUTPUT_CLOBBER,  // '>|', TOKEN_CLOBBER
    IO_REDIR_OUTPUT_APPEND,   // '>>', TOKEN_DGREAT
    IO_REDIR_HEREDOC,     // '<<' and '<<-', TOKEN_DLESS and TOKEN_DLESSDASH
    IO_REDIR_INPUT_DUP,   // '<&', TOKEN_LESSAND
    IO_REDIR_OUTPUT_DUP,  // '>&', TOKEN_GREATAND
    IO_REDIR_INOUT,       // '<>', TOKEN_LESSGREAT
} io_redir_type_t;

typedef enum il_type_e {
    // No parameter
    IL_ASSIGN_WORD,      // Make word@top an assignment word
    IL_COMPOSE_COMMAND,  // Make an executable command till first CMDINIT
    IL_COMPOSE_IOREDIR,  // Make an IO-redir instruction
    IL_COMPOSE_WORD,     // Make an complete word till first WORDINIT
    IL_EXEC_BACKGROUND,  // Execute the pipeline in the background
    IL_EXEC_PIPELINE,    // Execute a pipeline
    IL_EXPAND_PARAM,     // Do parameter expansion
    IL_PENDING_NOT,      // Inverse next EXEC_*'s result
    IL_PIPELINE_LINK,    // Create a pipeline between two commands
    IL_PUSH_CMDINIT,     // Push a CMDINIT to the stack
    IL_PUSH_WORDINIT,    // Push a WORDINIT to the stack

    // 1 string parameter
    IL_PUSH_NAME,        // Push a name to the stack
    IL_PUSH_PARTIAL,     // Push a partial word to the stack

    // 1 integer parameter
    IL_PUSH_FD,          // Push a file descriptor to the stack
    IL_PUSH_REDIR,     // Push IO-redir type to the stack
} il_type_t;

typedef struct il_list_s il_list_t;

const char *io_redir_type_name(io_redir_type_t redir);
const char *il_type_name(il_type_t type);

bool il_list_init(il_list_t *list);
void il_list_free(il_list_t *list);
bool il_list_clear(il_list_t *list);
bool il_list_valid(const il_list_t * list);
int il_list_size(const il_list_t * list);
bool il_list_push(il_list_t *list, il_type_t type);
bool il_list_pushs(il_list_t *list, il_type_t type, const char *payload);
bool il_list_pushi(il_list_t *list, il_type_t type, int payload);
void il_list_dump(const il_list_t *list);

#endif // IL_H
