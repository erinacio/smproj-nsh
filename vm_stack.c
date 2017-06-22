#define INCLUDE_VM_INTERNAL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "vm_stack.h"

static const int min_cap = 256;

static bool vm_stack_try_expand(vm_stack_t *stack)
{
    if (stack->size + 1 < stack->capacity) return true;
    int new_cap = stack->capacity + stack->capacity / 2;
    if (new_cap < min_cap) return false;
    vm_entry_t **new_arr = realloc(stack->entries, new_cap);
    if (new_arr == NULL) return false;
    stack->entries = new_arr;
    stack->capacity = new_cap;
    return true;
}

static void vm_stack_try_shrink(vm_stack_t *stack)
{
    if (stack->size > stack->capacity / 4) return;
    int new_cap = stack->capacity / 2;
    if (new_cap < min_cap) new_cap = min_cap;
    vm_entry_t **new_arr = realloc(stack->entries, new_cap);
    if (new_arr == NULL) return;
    stack->entries = new_arr;
    stack->capacity = new_cap;
}

bool vm_stack_init(vm_stack_t *stack)
{
    if (stack == NULL || stack->entries != NULL) return false;
    stack->entries = malloc(sizeof(vm_entry_t *) * min_cap);
    if (stack->entries == NULL) return false;
    stack->capacity = min_cap;
    stack->size = 0;
    return true;
}

void vm_stack_free(vm_stack_t *stack)
{
    if (stack == NULL) return;
    if (stack->entries != NULL) {
        for (int i = 0; i < stack->size; ++i) free_vm_entry(stack->entries[i]);
        free(stack->entries);
        stack->entries = NULL;
    }
    stack->capacity = 0;
    stack->size = 0;
}

bool vm_stack_valid(vm_stack_t *stack)
{
    return stack != NULL && stack->entries != NULL
            && stack->capacity >= min_cap && stack->size >= 0;
}

bool vm_stack_push(vm_stack_t *stack, vm_entry_t *entry)
{
    if (!vm_stack_valid(stack)) return false;
    if (!vm_stack_try_expand(stack)) return false;
    stack->entries[stack->size++] = entry;
    return true;
}

vm_entry_t *vm_stack_pop(vm_stack_t *stack)
{
    if (!vm_stack_valid(stack)) return NULL;
    if (stack->size == 0) return NULL;
    vm_entry_t *ret = stack->entries[--stack->size];
    vm_stack_try_shrink(stack);
    return ret;
}

vm_entry_t *vm_stack_get(vm_stack_t *stack, int index)
{
    if (!vm_stack_valid(stack)) return NULL;
    if (index < 0) index = stack->size + index;
    if (index < 0) return NULL;
    if (index >= stack->size) return NULL;
    return stack->entries[index];
}
