#define INCLUDE_VM_INTERNAL
#include <stdio.h>
#include <stdlib.h>
#define _XOPEN_SOURCE 500
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "il.h"
#include "vm_entry.h"
#include "vm_stack.h"
#include "vm.h"
#include "cfuhash.h"
#include "il_t.inc.h"
#include "utils.h"
#include "exec.h"
#include "states.h"
#include <reader.h>

typedef struct vm_s {
    vm_stack_t stack;
    cfuhash_table_t *assigns;
    int recent_ret;
} vm_t;

const char *vm_error_name(vm_error_t vme)
{
    switch (vme) {
    case VM_NO_ERROR:
        return "No error";
    case  VM_ERR_INTERNAL:
        return "Internal error";
    case VM_ERR_PARAMETER:
        return "Invalid function parameter";
    case VM_ERR_TYPE_MISMATCH:
        return "Type mismatch";
    case VM_ERR_UNKNOWN_IL:
        return "Unknown IL";
    case VM_ERR_NOT_IMPLEMENTED:
        return "IL function not implemented";
    case VM_ERR_OVERFLOW:
        return "Overflow";
    case VM_ERR_INVALID_VALUE:
        return "Invalid value";
    default:
        return "Unknown error";
    }
}

vm_t *vm_new(void)
{
    vm_t *vm = malloc(sizeof(vm_t));
    if (vm == NULL) return NULL;

    vm->assigns = cfuhash_new();
    if (vm->assigns == NULL) {
        free(vm);
        return NULL;
    }

    vm->stack.entries = NULL;
    vm->stack.capacity = 0;
    vm->stack.size = 0;

    if (!vm_stack_init(&vm->stack)) {
        cfuhash_destroy(vm->assigns);
        free(vm);
        return NULL;
    }

    return vm;
}

void vm_free(vm_t *vm)
{
    if (vm == NULL) return;
    if (vm->assigns != NULL) cfuhash_destroy(vm->assigns);
    vm_stack_free(&vm->stack);
    free(vm);
}

bool vm_valid(vm_t *vm)
{
    return vm != NULL && vm->assigns != NULL && vm_stack_valid(&vm->stack);
}

bool vm_clear(vm_t *vm)
{
    vm_stack_free(&vm->stack);
    return vm_stack_init(&vm->stack);
}

static vm_error_t vm_try_push(vm_t *vm, vm_entry_t *e)
{
    if (e == NULL) return VM_ERR_INTERNAL;
    if (!vm_stack_push(&vm->stack, e)) {
        free_vm_entry(e);
        return VM_ERR_INTERNAL;
    }
    return VM_NO_ERROR;
}

static vm_error_t vm_push_no_param(vm_t *vm, il_type_t type)
{
    vm_entry_type_t etype;
    switch (type) {
    case IL_PENDING_NOT:   etype = VM_ENTRY_PENDING_NOT; break;
    case IL_PUSH_CMDINIT:  etype = VM_ENTRY_CMDINIT;     break;
    case IL_PUSH_WORDINIT: etype = VM_ENTRY_WORDINIT;    break;
    default: return VM_ERR_INTERNAL;
    }
    if (etype == VM_ENTRY_PENDING_NOT) return VM_ERR_NOT_IMPLEMENTED;
    vm_entry_t *e = make_vm_entry(etype);
    return vm_try_push(vm, e);
}

static vm_error_t vm_push_int(vm_t *vm, il_type_t type, int payload)
{
    vm_entry_type_t etype;
    switch (type) {
    case IL_PUSH_FD:    etype = VM_ENTRY_FD; break;
    case IL_PUSH_REDIR: etype = VM_ENTRY_REDIR; break;
    default: return VM_ERR_INTERNAL;
    }
    vm_entry_t *e = make_vm_entry_int(etype, payload);
    return vm_try_push(vm, e);
}

static vm_error_t vm_push_str(vm_t *vm, il_type_t type, const char *payload)
{
    vm_entry_type_t etype;
    switch (type) {
    case IL_PUSH_PARTIAL: etype = VM_ENTRY_PARTIAL; break;
    case IL_PUSH_NAME:    etype = VM_ENTRY_NAME;    break;
    default: return VM_ERR_INTERNAL;
    }
    vm_entry_t *e = make_vm_entry_str(etype, payload);
    return vm_try_push(vm, e);
}

#define VM_ENTRY_ASSERT(x, y) \
    do { \
        vm_entry_t *_ = (vm_entry_t *)(x); \
        if (_ == NULL || _->type != (y)) { \
            free_vm_entry(_); \
            return VM_ERR_TYPE_MISMATCH; \
        } \
    } while (0)

static vm_error_t vm_assign_word(vm_t *vm)
{
    vm_entry_str_t *word = (vm_entry_str_t *)vm_stack_pop(&vm->stack);
    VM_ENTRY_ASSERT(word, VM_ENTRY_WORD);

    char *var = word->pl_str;
    while (*var != '\0' && *var != '=') ++var;
    if (*var == '\0') {
        free_vm_entry((vm_entry_t *)word);
        return VM_ERR_INVALID_VALUE;
    }
    *var++ = '\0';

    vm_entry_t *e = make_vm_entry_assign(word->pl_str, var);
    free_vm_entry((vm_entry_t *)word);
    if (e == NULL) return VM_ERR_INTERNAL;
    if (!vm_stack_push(&vm->stack, e)) {
        free_vm_entry(e);
        return VM_ERR_INTERNAL;
    }

    return VM_NO_ERROR;
}

static vm_error_t vm_compose_command(vm_t *vm)
{
    int cmd_init_i;
    for (cmd_init_i = vm->stack.size - 1; cmd_init_i >= 0; --cmd_init_i) {
        vm_entry_type_t type = vm->stack.entries[cmd_init_i]->type;
        if (type != VM_ENTRY_WORD && type != VM_ENTRY_ASSIGN_WORD
                && type != VM_ENTRY_IOREDIR) {
            break;
        }
    }
    if (cmd_init_i < 0) return VM_ERR_TYPE_MISMATCH;
    vm_entry_t *cmd_init = vm->stack.entries[cmd_init_i];
    VM_ENTRY_ASSERT(cmd_init, VM_ENTRY_CMDINIT);
    free_vm_entry(cmd_init);

    int arg_count = 0, assign_count = 0, ioredir_count = 0;
    for (int i = cmd_init_i + 1; i < vm->stack.size; ++i) {
        switch (vm->stack.entries[i]->type) {
        case VM_ENTRY_WORD:        ++arg_count;     break;
        case VM_ENTRY_ASSIGN_WORD: ++assign_count;  break;
        case VM_ENTRY_IOREDIR:     ++ioredir_count; break;
        default: return VM_ERR_TYPE_MISMATCH;
        }
    }
    if (assign_count < 0 || arg_count < 0 || ioredir_count < 0) {
        return VM_ERR_OVERFLOW;
    }

    vm_entry_command_t *command = malloc(sizeof(vm_entry_command_t));
    if (command == NULL) return VM_ERR_INTERNAL;
    command->type = VM_ENTRY_COMMAND;
    command->pipe_in = command->pipe_out = -1;

    size_t args_size = sizeof(vm_entry_str_t *) * (arg_count + 1);
    size_t assigns_size = sizeof(vm_entry_assign_t *) * (assign_count + 1);
    size_t ioredirs_size = sizeof(vm_entry_ioredir_t *) * (ioredir_count + 1);

    vm_entry_str_t **args = malloc(args_size);
    vm_entry_assign_t **assigns = malloc(assigns_size);
    vm_entry_ioredir_t **ioredirs = malloc(ioredirs_size);

    if (args == NULL || assigns == NULL || ioredirs == NULL) {
        if (args != NULL)     free(args);
        if (assigns != NULL)  free(assigns);
        if (ioredirs != NULL) free(ioredirs);
        free(command);
    }

    for (int i = 0; i <= arg_count; ++i) args[i] = NULL;
    for (int i = 0; i <= assign_count; ++i) assigns[i] = NULL;
    for (int i = 0; i <= ioredir_count; ++i) ioredirs[i] = NULL;

    for (int i = cmd_init_i + 1, arg_i = 0, ass_i = 0, ior_i = 0;
         i < vm->stack.size; ++i) {
        vm_entry_t *e = vm->stack.entries[i];
        switch (e->type) {
        case VM_ENTRY_WORD:
            args[arg_i++] = (vm_entry_str_t *)e;
            break;
        case VM_ENTRY_ASSIGN_WORD:
            assigns[ass_i++] = (vm_entry_assign_t *)e;
            break;
        case VM_ENTRY_IOREDIR:
            ioredirs[ior_i++] = (vm_entry_ioredir_t *)e;
            break;
        default:
            continue;
        }
    }

    command->args = args;
    command->assigns = assigns;
    command->redirs = ioredirs;

    vm->stack.size = cmd_init_i;

    if (!vm_stack_push(&vm->stack, (vm_entry_t *)command)) {
        free_vm_entry((vm_entry_t *)command);
        return VM_ERR_INTERNAL;
    }

    return VM_NO_ERROR;
}

static vm_error_t vm_compose_ioredir(vm_t *vm)
{
    vm_entry_int_t *re = (vm_entry_int_t *)vm_stack_pop(&vm->stack);
    VM_ENTRY_ASSERT(re, VM_ENTRY_REDIR);
    io_redir_type_t redir = (io_redir_type_t)re->pl_int;
    free_vm_entry((vm_entry_t *)re);

    vm_entry_int_t *fde = (vm_entry_int_t *)vm_stack_pop(&vm->stack);
    VM_ENTRY_ASSERT(fde, VM_ENTRY_FD);
    int fd = fde->pl_int;
    free_vm_entry((vm_entry_t *)fde);

    if (redir == IO_REDIR_INPUT_DUP || redir == IO_REDIR_OUTPUT_DUP) {
        vm_entry_int_t *fd2e = (vm_entry_int_t *)vm_stack_pop(&vm->stack);
        VM_ENTRY_ASSERT(fd2e, VM_ENTRY_FD);
        int fd_dup = fd2e->pl_int;
        free_vm_entry((vm_entry_t *)fd2e);

        vm_entry_t *e = make_vm_entry_ioredir_fd(redir, fd, fd_dup);
        if (e == NULL) return VM_ERR_INTERNAL;
        if (!vm_stack_push(&vm->stack, e)) {
            free_vm_entry(e);
            return VM_ERR_INTERNAL;
        }

    } else if (redir != IO_REDIR_HEREDOC) {
        vm_entry_str_t *pe = (vm_entry_str_t *)vm_stack_pop(&vm->stack);
        VM_ENTRY_ASSERT(pe, VM_ENTRY_WORD);

        vm_entry_t *e = make_vm_entry_ioredir_path(redir, fd, pe->pl_str);
        free_vm_entry((vm_entry_t *)pe);
        if (e == NULL) return VM_ERR_INTERNAL;
        if (!vm_stack_push(&vm->stack, e)) {
            free_vm_entry(e);
            return VM_ERR_INTERNAL;
        }
    } else {
        return VM_ERR_NOT_IMPLEMENTED;
    }

    return VM_NO_ERROR;
}

static vm_error_t vm_compose_word(vm_t *vm)
{
    int word_init_i;
    for (word_init_i = vm->stack.size - 1; word_init_i >= 0; --word_init_i) {
        vm_entry_type_t type = vm->stack.entries[word_init_i]->type;
        if (type != VM_ENTRY_PARTIAL) break;
    }
    if (word_init_i < 0) return VM_ERR_TYPE_MISMATCH;
    vm_entry_t *word_init = vm->stack.entries[word_init_i];
    VM_ENTRY_ASSERT(word_init, VM_ENTRY_WORDINIT);
    free_vm_entry(word_init);

    ptrdiff_t total_len = 0;
    for (int i = word_init_i + 1; i < vm->stack.size; ++i) {
        vm_entry_str_t *e = (vm_entry_str_t *)(vm->stack.entries[i]);
        ptrdiff_t this_len = strlen(e->pl_str);
        if (this_len < 0) return VM_ERR_OVERFLOW;
        total_len += this_len;
        if (total_len < 0) return VM_ERR_OVERFLOW;
    }

    bool tilde = ((vm_entry_str_t *)(vm->stack.entries[word_init_i + 1]))->pl_str[0] == '~';
    vm_entry_str_t *word = malloc(sizeof(vm_entry_str_t) + (size_t)total_len + 1);
    if (word == NULL) return VM_ERR_INTERNAL;
    word->type = VM_ENTRY_WORD;
    char *payload = word->pl_str;
    for (int i = word_init_i + 1; i < vm->stack.size; ++i) {
        vm_entry_str_t *e = (vm_entry_str_t *)(vm->stack.entries[i]);

        bool single_quote = false, backslash = false;
        for (const char *p = e->pl_str; *p != '\0'; ++p) {
            if (*p == '\'' && (single_quote || !backslash)) {
                single_quote = !single_quote;
            } else if (*p == '\\' && single_quote == false && backslash == false) {
                backslash = true;
            } else {
                backslash = false;
                *payload++ = *p;
            }
        }
        *payload = '\0';

        free_vm_entry((vm_entry_t *)e);
    }
    word->pl_str[total_len] = '\0';

    while (tilde) {
        char *path = tilde_expand(word->pl_str);
        if (path == NULL) break;
        vm_entry_str_t *new_word = realloc(word, sizeof(vm_entry_str_t) + strlen(path) + 1);
        if (new_word == NULL) {
            free(path);
            break;
        }
        word = new_word;
        strcpy(word->pl_str, path);
        free(path);
        break;
    }

    vm->stack.size = word_init_i;
    if (!vm_stack_push(&vm->stack, (vm_entry_t *)word)) {
        free_vm_entry((vm_entry_t *)word);
        return VM_ERR_INTERNAL;
    }

    return VM_NO_ERROR;
}

static vm_error_t vm_exec_background(vm_t *vm)
{
    vm_entry_t *e = vm_stack_pop(&vm->stack);
    if (e == NULL) return VM_ERR_INTERNAL;

    switch (e->type) {
    case VM_ENTRY_COMMAND:
        exec_command((vm_entry_command_t *)e, NULL, BACKGROUND);
        free_vm_entry(e);
        return VM_NO_ERROR;
    case VM_ENTRY_PIPELINE:
        exec_pipeline((vm_entry_pipeline_t *)e, NULL, BACKGROUND);
        free_vm_entry(e);
        return VM_NO_ERROR;
    default:
        return VM_ERR_TYPE_MISMATCH;
    }
}

static void add_assigns(vm_t *vm, vm_entry_command_t *e)
{
    if (e->assigns == NULL) return;
    for (vm_entry_assign_t **pa = e->assigns; *pa != NULL; ++pa) {
        vm_entry_assign_t *a = *pa;
        void *old = cfuhash_put(vm->assigns, a->pl_name, strdup(a->pl_val));
        if (old != NULL) free(old);
        if (strcmp(a->pl_name, "HISTSIZE") == 0) {
            reader_set_histsize(a->pl_val);
        }
    }
}

static vm_error_t vm_exec_pipeline(vm_t *vm)
{
    vm_entry_t *e = vm_stack_pop(&vm->stack);
    if (e == NULL) return VM_ERR_INTERNAL;

    switch (e->type) {
    case VM_ENTRY_COMMAND: {
        vm_entry_command_t *c = (vm_entry_command_t *)e;
        if (c->args == NULL || c->args[0] == NULL) {
            add_assigns(vm, c);
        } else {
            exec_command((vm_entry_command_t *)e, &vm->recent_ret, FOREGROUND);
        }
        free_vm_entry(e);
        return VM_NO_ERROR;
    } case VM_ENTRY_PIPELINE:
        exec_pipeline((vm_entry_pipeline_t *)e, &vm->recent_ret, FOREGROUND);
        free_vm_entry(e);
        return VM_NO_ERROR;
    default:
        return VM_ERR_TYPE_MISMATCH;
    }
}

static vm_error_t vm_expand_param(vm_t *vm)
{
    vm_entry_str_t *e = (vm_entry_str_t *)(vm_stack_pop(&vm->stack));
    if (e == NULL || e->type != VM_ENTRY_NAME) return VM_ERR_TYPE_MISMATCH;

    const char *partial = NULL;
    if (cfuhash_exists(vm->assigns, e->pl_str)) {
        partial = cfuhash_get(vm->assigns, e->pl_str);
    } else if (getenv(e->pl_str) != NULL) {
        partial = getenv(e->pl_str);
    }
    if (partial == NULL) partial = "";

    free_vm_entry((vm_entry_t *)e);
    vm_entry_t *ne = make_vm_entry_str(VM_ENTRY_PARTIAL, partial);
    if (ne == NULL) return VM_ERR_INTERNAL;

    if (!vm_stack_push(&vm->stack, ne)) {
        free_vm_entry(ne);
        return VM_ERR_INTERNAL;
    }

    return VM_NO_ERROR;
}

static vm_error_t vm_pipeline_link(vm_t *vm)
{
    vm_entry_command_t *right = (vm_entry_command_t *)vm_stack_pop(&vm->stack);
    VM_ENTRY_ASSERT(right, VM_ENTRY_COMMAND);
    vm_entry_t *left = vm_stack_pop(&vm->stack);
    if (left == NULL || (left->type != VM_ENTRY_COMMAND && left->type != VM_ENTRY_PIPELINE)) {
        free_vm_entry((vm_entry_t *)right);
        free_vm_entry(left);
        return VM_ERR_TYPE_MISMATCH;
    }

    vm_entry_t *pipe = make_vm_entry_pipeline(left, right);
    if (pipe == NULL) {
        free_vm_entry((vm_entry_t *)right);
        free_vm_entry(left);
        return VM_ERR_INTERNAL;
    }

    if (!vm_stack_push(&vm->stack, pipe)) {
        free_vm_entry(pipe);
        return VM_ERR_INTERNAL;
    }

    return VM_NO_ERROR;
}

vm_error_t vm_exec1(vm_t *vm, il_t *il)
{
    switch (il->type) {
    case IL_ASSIGN_WORD:
        return vm_assign_word(vm);
    case IL_COMPOSE_COMMAND:
        return vm_compose_command(vm);
    case IL_COMPOSE_IOREDIR:
        return vm_compose_ioredir(vm);
    case IL_COMPOSE_WORD:
        return vm_compose_word(vm);
    case IL_EXEC_BACKGROUND:
        return vm_exec_background(vm);
    case IL_EXEC_PIPELINE:
        return vm_exec_pipeline(vm);
    case IL_EXPAND_PARAM:
        return vm_expand_param(vm);
    case IL_PIPELINE_LINK:
        return vm_pipeline_link(vm);
    case IL_PENDING_NOT:
    case IL_PUSH_CMDINIT:
    case IL_PUSH_WORDINIT:
        return vm_push_no_param(vm, il->type);
    case IL_PUSH_NAME:
    case IL_PUSH_PARTIAL:
        return vm_push_str(vm, il->type, ((il_param_str_t *)il)->pl_str);
    case IL_PUSH_FD:
    case IL_PUSH_REDIR:
        return vm_push_int(vm, il->type, ((il_param_int_t *)il)->pl_int);
    default:
        return VM_ERR_UNKNOWN_IL;
    }
}

vm_error_t vm_exec(vm_t *vm, il_list_t *ils)
{
    if (!vm_valid(vm) || !il_list_valid(ils)) return VM_ERR_PARAMETER;
    if (state_debug) {
        il_list_dump(ils);
    }
    for (int i = 0; i < ils->size; ++i) {
        il_t *il = ils->array[i];
        vm_error_t err = vm_exec1(vm, il);
        if (state_debug) {
            putchar('\n');
            print_il(il);
            vm_dump(vm);
        }
        if (err != VM_NO_ERROR) {
            printf("VM error: %s (%s)\n", vm_error_name(err),
                   il_type_name(ils->array[i]->type));
            return err;
        }
    }
    return VM_NO_ERROR;
}

static int vm_assigns_foreach(void *key, size_t key_size, void *data, size_t data_size, void *arg)
{
    UNUSED_VAR(key_size); UNUSED_VAR(data_size); UNUSED_VAR(arg);
    printf("    %s=", (char *)key);
    print_str_repr((const char *)data, -1);
    putchar('\n');
    return 0;
}

void vm_dump(vm_t *vm)
{
    if (!vm_valid(vm)) {
        printf("<invalid vm at %p>\n", vm);
        return;
    }
    printf("<<<<<======= VM DUMP BEGIN OF %p =======<<<<<\n", vm);
    puts("vm assigns:");
    cfuhash_foreach(vm->assigns, &vm_assigns_foreach, NULL);
    puts("vm stack:");
    for (int i = 0; i < vm->stack.size; ++i) {
        print_vm_entry(vm->stack.entries[i], 4);
    }
    printf(">>>>>======== VM DUMP END OF %p ========>>>>>\n", vm);
}
