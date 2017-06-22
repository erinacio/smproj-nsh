#ifndef INCLUDE_VM_INTERNAL
# error This file is not intended to be included outside vm.c.
#endif

#ifndef VM_ENTRY_H
#define VM_ENTRY_H

#include <stdbool.h>
#include "il.h"

typedef enum vm_entry_type_e {
    // Zero payload type
    VM_ENTRY_CMDINIT,
    VM_ENTRY_WORDINIT,
    VM_ENTRY_PENDING_NOT,

    // Primitive payload type
    VM_ENTRY_NAME,
    VM_ENTRY_PARTIAL,
    VM_ENTRY_FD,
    VM_ENTRY_REDIR,
    VM_ENTRY_WORD,

    // Complex payload type
    VM_ENTRY_ASSIGN_WORD,
    VM_ENTRY_IOREDIR,
    VM_ENTRY_COMMAND,
    VM_ENTRY_PIPELINE,
} vm_entry_type_t;

typedef struct vm_entry_s {
    vm_entry_type_t type;
} vm_entry_t;

typedef struct vm_entry_int_s {
    vm_entry_type_t type;
    int pl_int;
} vm_entry_int_t;

typedef struct vm_entry_str_s {
    vm_entry_type_t type;
    char pl_str[];
} vm_entry_str_t;

typedef struct vm_entry_assign_s {
    vm_entry_type_t type;
    char *pl_val;
    char *pl_name;
} vm_entry_assign_t;

typedef struct vm_entry_ioredir_s {
    vm_entry_type_t type;
    io_redir_type_t redir_type;
    int pl_fd;
    int pl_fd2;
    char pl_path[];
} vm_entry_ioredir_t;

typedef struct vm_entry_command_s {
    vm_entry_type_t type;
    vm_entry_str_t **args;
    vm_entry_assign_t **assigns;
    vm_entry_ioredir_t **redirs;
    int pipe_in;
    int pipe_out;
} vm_entry_command_t;

typedef struct vm_entry_pipeline_s {
    vm_entry_type_t type;
    vm_entry_command_t *commands[];
} vm_entry_pipeline_t;

const char *vm_entry_type_name(vm_entry_type_t type);

bool is_vm_entry_no_payload(vm_entry_type_t type);
bool is_vm_entry_int(vm_entry_type_t type);
bool is_vm_entry_str(vm_entry_type_t type);

vm_entry_t *make_vm_entry(vm_entry_type_t type);
vm_entry_t *make_vm_entry_int(vm_entry_type_t type, int payload);
vm_entry_t *make_vm_entry_str(vm_entry_type_t type, const char *payload);
vm_entry_t *make_vm_entry_assign(const char *name, const char *val);
vm_entry_t *make_vm_entry_ioredir_fd(io_redir_type_t redir_type, int fd, int fd2);
vm_entry_t *make_vm_entry_ioredir_path(io_redir_type_t redir_type, int fd, const char *path);
vm_entry_t *make_vm_entry_command(vm_entry_str_t **args, vm_entry_assign_t **assigns, vm_entry_ioredir_t **redirs);
vm_entry_t *make_vm_entry_pipeline(vm_entry_t *left, vm_entry_command_t *right);

void free_vm_entry_assign(vm_entry_assign_t *e);
void free_vm_entry_command(vm_entry_command_t *e);
void free_vm_entry_pipeline(vm_entry_pipeline_t *e);
void free_vm_entry(vm_entry_t *e);

void print_vm_entry(vm_entry_t *e, int indent);

#endif // VM_ENTRY_H
