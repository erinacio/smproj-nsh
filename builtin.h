#ifndef BUILTIN_H
#define BUILTIN_H

#include <stdbool.h>

typedef struct vm_entry_command_s vm_entry_command_t;

bool is_builtin(vm_entry_command_t *cmd);
int call_builtin(vm_entry_command_t *cmd);
int builtin_cd(vm_entry_command_t *cmd);
int builtin_exit(vm_entry_command_t *cmd);
int builtin_alias(vm_entry_command_t *cmd);
int builtin_debug(vm_entry_command_t *cmd);
int builtin_export(vm_entry_command_t *cmd);
int builtin_unalias(vm_entry_command_t *cmd);
int builtin_history(vm_entry_command_t *cmd);
int builtin_unexport(vm_entry_command_t *cmd);

#endif // BUILTIN_H
