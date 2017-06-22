#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "il.h"
#include "il_t.inc.h"
#include "utils.h"

const char *io_redir_type_name(io_redir_type_t redir)
{
#define _MKENT(x) case x: return #x
    switch (redir) {
    _MKENT(IO_REDIR_UNKNOWN);
    _MKENT(IO_REDIR_INPUT);
    _MKENT(IO_REDIR_OUTPUT);
    _MKENT(IO_REDIR_OUTPUT_CLOBBER);
    _MKENT(IO_REDIR_OUTPUT_APPEND);
    _MKENT(IO_REDIR_HEREDOC);
    _MKENT(IO_REDIR_INPUT_DUP);
    _MKENT(IO_REDIR_OUTPUT_DUP);
    _MKENT(IO_REDIR_INOUT);
    default: return "IO_REDIR_UNKNOWN";
    }
#undef _MKENT
}

const char *il_type_name(il_type_t type)
{
#define _MKENT(x) case IL_##x: return #x
    switch (type) {
    _MKENT(ASSIGN_WORD);
    _MKENT(COMPOSE_COMMAND);
    _MKENT(COMPOSE_IOREDIR);
    _MKENT(COMPOSE_WORD);
    _MKENT(EXEC_BACKGROUND);
    _MKENT(EXEC_PIPELINE);
    _MKENT(EXPAND_PARAM);
    _MKENT(PENDING_NOT);
    _MKENT(PIPELINE_LINK);
    _MKENT(PUSH_CMDINIT);
    _MKENT(PUSH_WORDINIT);
    _MKENT(PUSH_NAME);
    _MKENT(PUSH_PARTIAL);
    _MKENT(PUSH_FD);
    _MKENT(PUSH_REDIR);
    default: return "????????";
    }

#undef _MKENT
}

static const int min_cap = 128;

bool il_list_init(il_list_t *list)
{
    if (list == NULL || list->array != NULL) return false;
    list->array = malloc(sizeof(il_t *) * min_cap);
    if (list->array == NULL) return false;
    list->capacity = min_cap;
    list->size = 0;
    return true;
}

void il_list_free(il_list_t *list)
{
    if (list == NULL || list->array == NULL) return;
    for (int i = 0; i < list->size; ++i) {
        free(list->array[i]);
    }
    free(list->array);
    list->array = NULL;
    list->capacity = 0;
    list->size = 0;
}

bool il_list_clear(il_list_t *list)
{
    if (list->capacity > min_cap) list->capacity = min_cap;
    il_t **new_arr = realloc(list->array, list->capacity * sizeof(il_t *));
    if (new_arr == NULL) return false;
    list->array = new_arr;
    list->size = 0;
    return true;
}

bool il_list_valid(const il_list_t *list)
{
    return list != NULL && list->array != NULL
            && list->capacity >= min_cap && list->size >= 0;
}

int il_list_size(const il_list_t *list)
{
    if (list == NULL || list->array == NULL) return 0;
    return list->size;
}

static bool try_expand_list(il_list_t *list)
{
    if (list->size + 1 < list->capacity) return true;
    int new_cap = list->capacity * 2;
    if (new_cap < 0) return false;
    il_t **new_arr = realloc(list->array, list->capacity);
    if (new_arr == NULL) return false;
    list->capacity = new_cap;
    list->array = new_arr;
    return true;
}

static bool il_list_raw_push(il_list_t *list, il_t *il)
{
    if (il == NULL) return false;
    if (!try_expand_list(list)) return false;

    list->array[list->size++] = il;
    return true;
}

typedef enum il_type_type_e {
    IL_TYPE_INVALID,
    IL_TYPE_NO_PARAM,
    IL_TYPE_STR_PARAM,
    IL_TYPE_INT_PARAM,
} il_type_type_t;

static il_type_type_t get_il_type_type(il_type_t type)
{
    switch (type) {
    case IL_ASSIGN_WORD:
    case IL_COMPOSE_COMMAND:
    case IL_COMPOSE_IOREDIR:
    case IL_COMPOSE_WORD:
    case IL_EXEC_BACKGROUND:
    case IL_EXEC_PIPELINE:
    case IL_EXPAND_PARAM:
    case IL_PENDING_NOT:
    case IL_PIPELINE_LINK:
    case IL_PUSH_CMDINIT:
    case IL_PUSH_WORDINIT:
        return IL_TYPE_NO_PARAM;
    case IL_PUSH_NAME:
    case IL_PUSH_PARTIAL:
        return IL_TYPE_STR_PARAM;
    case IL_PUSH_FD:
    case IL_PUSH_REDIR:
        return IL_TYPE_INT_PARAM;
    default:
        return IL_TYPE_INVALID;
    }
}

bool il_list_push(il_list_t *list, il_type_t type)
{
    if (!il_list_valid(list) || get_il_type_type(type) != IL_TYPE_NO_PARAM) return false;

    il_t *il = malloc(sizeof(il_t));
    if (il == NULL) return false;
    il->type = type;
    if (!il_list_raw_push(list, il)) {
        free(il);
        return false;
    }
    return true;
}

bool il_list_pushs(il_list_t *list, il_type_t type, const char *payload)
{
    if (!il_list_valid(list) || payload == NULL || get_il_type_type(type) != IL_TYPE_STR_PARAM) return false;
    size_t pl_len = strlen(payload);
    if (pl_len >= PTRDIFF_MAX) return false;

    il_param_str_t *il = malloc(sizeof(il_param_str_t) + pl_len + 1);
    if (il == NULL) return false;
    il->type = type;

    memcpy(il->pl_str, payload, pl_len);
    il->pl_str[pl_len] = '\0';

    if (!il_list_raw_push(list, (il_t *)il)) {
        free(il);
        return false;
    }
    return true;
}

bool il_list_pushi(il_list_t *list, il_type_t type, int payload)
{
    if (!il_list_valid(list) || get_il_type_type(type) != IL_TYPE_INT_PARAM) return false;

    il_param_int_t *il = malloc(sizeof(il_param_int_t));
    if (il == NULL) return false;
    il->type = type;
    il->pl_int = payload;
    if (!il_list_raw_push(list, (il_t *)il)) {
        free(il);
        return false;
    }
    return true;
}

void print_il(il_t *il)
{
    printf("%-16s", il_type_name(il->type));
    switch (get_il_type_type(il->type)) {
    case IL_TYPE_NO_PARAM:
        printf("\n");
        break;
    case IL_TYPE_STR_PARAM:
        putchar(' ');
        print_str_repr(((il_param_str_t *)il)->pl_str, -1);
        putchar('\n');
        break;
    case IL_TYPE_INT_PARAM:
        printf(" %d\n", ((il_param_int_t *)il)->pl_int);
        break;
    default:
        break;
    }
}

void il_list_dump(const il_list_t *list)
{
    if (!il_list_valid(list)) {
        printf("<invalid list at %p>\n", list);
        return;
    }
    printf("<<<<<======= IL DUMP BEGIN OF %p =======<<<<<\n", list);
    for (int i = 0; i < list->size; ++i) {
        il_t *il = list->array[i];
        printf("    ");
        print_il(il);
    }
    printf(">>>>>======== IL DUMP END OF %p ========>>>>>\n", list);
}
