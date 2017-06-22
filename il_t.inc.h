#ifndef IL_T_INC_H
#define IL_T_INC_H

#include "il.h"

typedef struct il_s {
    il_type_t type;
} il_t;

typedef struct il_param_int_s {
    il_type_t type;
    int pl_int;
} il_param_int_t;

typedef struct il_param_str_s {
    il_type_t type;
    char pl_str[];
} il_param_str_t;

typedef struct il_list_s {
    il_t **array;
    int size;
    int capacity;
} il_list_t;

void print_il(il_t *il);

#endif // IL_T_INC_H
