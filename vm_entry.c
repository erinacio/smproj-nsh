#define INCLUDE_VM_INTERNAL
#define _XOPEN_SOURCE 500 // to use strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include "vm_entry.h"
#include "utils.h"

const char *vm_entry_type_name(vm_entry_type_t type)
{
#define _MKENT(x) case x: return #x
    switch (type) {
    _MKENT(VM_ENTRY_CMDINIT);
    _MKENT(VM_ENTRY_WORDINIT);
    _MKENT(VM_ENTRY_PENDING_NOT);
    _MKENT(VM_ENTRY_NAME);
    _MKENT(VM_ENTRY_PARTIAL);
    _MKENT(VM_ENTRY_FD);
    _MKENT(VM_ENTRY_REDIR);
    _MKENT(VM_ENTRY_WORD);
    _MKENT(VM_ENTRY_ASSIGN_WORD);
    _MKENT(VM_ENTRY_IOREDIR);
    _MKENT(VM_ENTRY_COMMAND);
    _MKENT(VM_ENTRY_PIPELINE);
    default: return "VM_ENTRY_UNKNOWN";
    }
#undef _MKENT
}

bool is_vm_entry_no_payload(vm_entry_type_t type)
{
    return type == VM_ENTRY_CMDINIT || type == VM_ENTRY_WORDINIT
            || type == VM_ENTRY_PENDING_NOT;
}

bool is_vm_entry_int(vm_entry_type_t type)
{
    return type == VM_ENTRY_FD || type == VM_ENTRY_REDIR;
}

bool is_vm_entry_str(vm_entry_type_t type)
{
    return type == VM_ENTRY_NAME || type == VM_ENTRY_PARTIAL
            || type == VM_ENTRY_WORD;
}

vm_entry_t *make_vm_entry(vm_entry_type_t type)
{
    if (!is_vm_entry_no_payload(type)) return NULL;
    vm_entry_t *e = malloc(sizeof(vm_entry_t));
    if (e == NULL) return NULL;
    e->type = type;
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_int(vm_entry_type_t type, int payload)
{
    if (!is_vm_entry_int(type)) return NULL;
    vm_entry_int_t *e = malloc(sizeof(vm_entry_int_t));
    if (e == NULL) return NULL;
    e->type = type;
    e->pl_int = payload;
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_str(vm_entry_type_t type, const char *payload)
{
    if (!is_vm_entry_str(type) || payload == NULL) return NULL;
    size_t pl_len = strlen(payload);
    if (pl_len > INT_MAX) return NULL;
    vm_entry_str_t *e = malloc(sizeof(vm_entry_str_t) + pl_len + 1);
    if (e == NULL) return NULL;
    e->type = type;
    memcpy(e->pl_str, payload, pl_len);
    e->pl_str[pl_len] = '\0';
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_assign(const char *name, const char *val)
{
    if (name == NULL || val == NULL) return NULL;
    vm_entry_assign_t *e = malloc(sizeof(vm_entry_assign_t));
    if (e == NULL) return NULL;
    e->type = VM_ENTRY_ASSIGN_WORD;
    e->pl_name = strdup(name);
    e->pl_val = strdup(val);
    if (e->pl_val == NULL || e->pl_name == NULL) {
        if (e->pl_name != NULL) free(e->pl_name);
        if (e->pl_val != NULL) free(e->pl_val);
        free(e);
        return NULL;
    }
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_ioredir_fd(io_redir_type_t redir_type, int fd, int fd2)
{
    if (redir_type != IO_REDIR_INPUT_DUP && redir_type != IO_REDIR_OUTPUT_DUP) return NULL;
    if (fd < 0 || fd2 < 0) return NULL;
    vm_entry_ioredir_t *e = malloc(sizeof(vm_entry_ioredir_t));
    if (e == NULL) return NULL;
    e->type = VM_ENTRY_IOREDIR;
    e->redir_type = redir_type;
    e->pl_fd = fd;
    e->pl_fd2 = fd2;
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_ioredir_path(io_redir_type_t redir_type, int fd, const char *path)
{
    if (redir_type == IO_REDIR_INPUT_DUP || redir_type == IO_REDIR_OUTPUT_DUP) return NULL;
    if (fd < 0 || path == NULL) return NULL;
    size_t pl_len = strlen(path);
    if (pl_len > INT_MAX) return NULL;
    vm_entry_ioredir_t *e = malloc(sizeof(vm_entry_ioredir_t) + pl_len + 1);
    if (e == NULL) return NULL;
    e->type = VM_ENTRY_IOREDIR;
    e->redir_type = redir_type;
    e->pl_fd = fd;
    memcpy(e->pl_path, path, pl_len);
    e->pl_path[pl_len] = '\0';
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_command(vm_entry_str_t **args, vm_entry_assign_t **assigns, vm_entry_ioredir_t **redirs)
{
    if (args == NULL || assigns == NULL || redirs == NULL) return NULL;
    vm_entry_command_t *e = malloc(sizeof(vm_entry_command_t));
    if (e == NULL) return NULL;
    e->type = VM_ENTRY_COMMAND;
    e->args = args;
    e->assigns = assigns;
    e->redirs = redirs;
    e->pipe_in = -1;
    e->pipe_out = -1;
    return (vm_entry_t *)e;
}

vm_entry_t *make_vm_entry_pipeline(vm_entry_t *left, vm_entry_command_t *right)
{
    if (left == NULL || right == NULL || !(left->type == VM_ENTRY_COMMAND
                                           || left->type == VM_ENTRY_PIPELINE)
            || right->type != VM_ENTRY_COMMAND) {
        return NULL;
    }

    if (left->type == VM_ENTRY_COMMAND) {
        vm_entry_pipeline_t *e = malloc(sizeof(vm_entry_pipeline_t *) + sizeof(vm_entry_command_t *) * 3);
        if (e == NULL) return NULL;
        e->type = VM_ENTRY_PIPELINE;
        e->commands[0] = (vm_entry_command_t *)left;
        e->commands[1] = right;
        e->commands[2] = NULL;
        return (vm_entry_t *)e;
    }

    int pipeline_len = 1;
    vm_entry_pipeline_t *pe = (vm_entry_pipeline_t *)left;
    for (vm_entry_command_t **c = pe->commands; *c != NULL; ++c) ++pipeline_len;
    if (pipeline_len < 0) return NULL;
    vm_entry_pipeline_t *e = realloc(left, sizeof(vm_entry_pipeline_t *) + sizeof(vm_entry_command_t *) * ((size_t)pipeline_len + 1));
    if (e == NULL) return NULL;
    e->commands[pipeline_len - 1] = right;
    e->commands[pipeline_len] = NULL;

    return (vm_entry_t *)e;
}

void free_vm_entry_assign(vm_entry_assign_t *e)
{
    if (e == NULL) return;
    if (e->pl_name != NULL) free(e->pl_name);
    if (e->pl_val != NULL) free(e->pl_val);
    free(e);
}

void free_vm_entry_command(vm_entry_command_t *e)
{
    if (e == NULL) return;
    if (e->args != NULL) {
        for (vm_entry_str_t **pe = e->args; *pe != NULL; ++pe) free(*pe);
        free(e->args);
    }
    if (e->assigns != NULL) {
        for (vm_entry_assign_t **pe = e->assigns; *pe != NULL; ++pe) {
            free_vm_entry_assign(*pe);
        }
        free(e->assigns);
    }
    if (e->redirs != NULL) {
        for (vm_entry_ioredir_t **pe = e->redirs; *pe != NULL; ++pe) free(*pe);
        free(e->redirs);
    }
    free(e);
}

void free_vm_entry_pipeline(vm_entry_pipeline_t *e)
{
    if (e == NULL) return;
    for (vm_entry_command_t **c = e->commands; *c != NULL; ++c) {
        free_vm_entry_command(*c);
    }
    free(e);
}

void free_vm_entry(vm_entry_t *e)
{
    if (e == NULL) return;
    switch (e->type) {
    case VM_ENTRY_ASSIGN_WORD:
        free_vm_entry_assign((vm_entry_assign_t *)e);
        return;
    case VM_ENTRY_COMMAND:
        free_vm_entry_command((vm_entry_command_t *)e);
        return;
    case VM_ENTRY_PIPELINE:
        free_vm_entry_pipeline((vm_entry_pipeline_t *)e);
        return;
    default:
        free(e);
        return;
    }
}

static void print_indent(int indent)
{
    while (indent--) putchar(' ');
}

static void print_vm_entry_command(vm_entry_t *e, int indent)
{
    vm_entry_command_t *ee = (vm_entry_command_t *)e;
    putchar('\n');

    print_indent(indent + 4);
    puts("args:");
    for (vm_entry_t **pa = (vm_entry_t **)ee->args; *pa != NULL; ++pa) {
        print_vm_entry(*pa, indent + 8);
    }

    print_indent(indent + 4);
    puts("assigns:");
    for (vm_entry_t **pa = (vm_entry_t **)ee->assigns; *pa != NULL; ++pa) {
        print_vm_entry(*pa, indent + 8);
    }

    print_indent(indent + 4);
    puts("redirs:");
    for (vm_entry_t **pr = (vm_entry_t **)ee->redirs; *pr != NULL; ++pr) {
        print_vm_entry(*pr, indent + 8);
    }
}

void print_vm_entry(vm_entry_t *e, int indent)
{
    print_indent(indent);
    if (e == NULL) puts("(null)");

    printf(vm_entry_type_name(e->type));
    if (is_vm_entry_no_payload(e->type)) {
        putchar('\n');
        return;
    } else if (is_vm_entry_int(e->type)) {
        printf("(%d)\n", ((vm_entry_int_t *)e)->pl_int);
        return;
    } else if (is_vm_entry_str(e->type)) {
        putchar('(');
        print_str_repr(((vm_entry_str_t *)e)->pl_str, -1);
        puts(")");
        return;
    }

    if (e->type == VM_ENTRY_ASSIGN_WORD) {
        vm_entry_assign_t *ee = (vm_entry_assign_t *)e;
        printf("{%s}<={", ee->pl_name);
        print_str_repr(ee->pl_val, -1);
        puts("}");

    } else if (e->type == VM_ENTRY_IOREDIR) {
        vm_entry_ioredir_t *ee = (vm_entry_ioredir_t *)e;
        if (ee->redir_type == IO_REDIR_INPUT_DUP || ee->redir_type == IO_REDIR_OUTPUT_DUP) {
            printf("{%d}<={%d}", ee->pl_fd2, ee->pl_fd);
        } else {
            putchar('{');
            print_str_repr(ee->pl_path, -1);
            printf("}<={%d}", ee->pl_fd);
        }
        printf("<-{%s}\n", io_redir_type_name(ee->redir_type));

    } else if (e->type == VM_ENTRY_COMMAND) {
        print_vm_entry_command(e, indent);

    } else if (e->type == VM_ENTRY_PIPELINE) {
        vm_entry_pipeline_t *ee = (vm_entry_pipeline_t *)e;
        putchar('\n');

        for (vm_entry_command_t **c = ee->commands; *c != NULL; ++c) {
            print_vm_entry((vm_entry_t *)(*c), indent + 4);
        }

        print_indent(indent);
        puts("END_ENTRY_PIPELINE");

    } else {
        putchar('\n');
    }
}
