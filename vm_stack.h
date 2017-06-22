#ifndef INCLUDE_VM_INTERNAL
# error This file is not intended to be included outside vm(_[a-z]+)?.c.
#endif

#ifndef VM_STACK_H
#define VM_STACK_H

#include <stdbool.h>
#include "vm_entry.h"

typedef struct vm_stack_s {
    vm_entry_t **entries;
    int size;
    int capacity;
} vm_stack_t;

bool vm_stack_init(vm_stack_t *stack);
void vm_stack_free(vm_stack_t *stack);
bool vm_stack_valid(vm_stack_t *stack);
bool vm_stack_push(vm_stack_t *stack, vm_entry_t *entry);
vm_entry_t *vm_stack_pop(vm_stack_t *stack);
vm_entry_t *vm_stack_get(vm_stack_t *stack, int index);

#endif // VM_STACK_H
