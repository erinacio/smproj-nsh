#define _XOPEN_SOURCE 500 // to use strdup

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils.h"
#include "cfuhash.h"

cfuhash_table_t *alias_table = NULL;

static void alias_delete(void)
{
    if (alias_table != NULL) cfuhash_destroy(alias_table);
}

void alias_init(void)
{
    if (alias_table != NULL) return;
    alias_table = cfuhash_new();
    if (alias_table == NULL) panic("Can't allocate alias table");
    atexit(&alias_delete);
}

bool alias_add(const char *name, const char *value)
{
    if (alias_table == NULL || name == NULL || value == NULL) return false;
    void *old_data = cfuhash_put(alias_table, name, strdup(value));
    if (old_data != NULL) free(old_data);
    return true;
}

const char *alias_get(const char *name)
{
    if (alias_table == NULL || name == NULL) return NULL;
    return cfuhash_get(alias_table, name);
}

bool alias_in(const char *name)
{
    if (alias_table == NULL || name == NULL) return false;
    return cfuhash_exists(alias_table, name);
}

bool alias_del(const char *name)
{
    if (alias_table == NULL || name == NULL) return false;
    void *old_data = cfuhash_delete(alias_table, name);
    if (old_data != NULL) free(old_data);
    return true;
}

static int alias_foreach(void *key, size_t key_size, void *data, size_t data_size, void *arg)
{
    UNUSED_VAR(key_size); UNUSED_VAR(data_size); UNUSED_VAR(arg);
    printf("%s=%s\n", (char *)key, (char *)data);
    return 0;
}

void alias_print_all()
{
    if (alias_table == NULL) return;
    cfuhash_foreach(alias_table, &alias_foreach, NULL);
}
