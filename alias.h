#ifndef ALIAS_H
#define ALIAS_H

#include <stdbool.h>

void alias_init(void);
bool alias_add(const char *name, const char *value);
const char *alias_get(const char *name);
bool alias_in(const char *name);
bool alias_del(const char *name);
void alias_print_all(void);

#endif // ALIAS_H
