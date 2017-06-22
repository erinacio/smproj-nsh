#ifndef VM_H
#define VM_H

#include "il.h"

typedef struct vm_s vm_t;

typedef enum vm_error_e {
    VM_NO_ERROR,
    VM_ERR_INTERNAL,
    VM_ERR_PARAMETER,
    VM_ERR_TYPE_MISMATCH,
    VM_ERR_UNKNOWN_IL,
    VM_ERR_NOT_IMPLEMENTED,
    VM_ERR_OVERFLOW,
    VM_ERR_INVALID_VALUE,
} vm_error_t;

const char *vm_error_name(vm_error_t vme);
vm_t *vm_new(void);
void vm_free(vm_t *vm);
bool vm_valid(vm_t *vm);
bool vm_clear(vm_t *vm);
vm_error_t vm_exec(vm_t *vm, il_list_t *ils);
void vm_dump(vm_t *vm);

#endif // VM_H
