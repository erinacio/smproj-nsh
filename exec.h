#ifndef EXEC_H
#define EXEC_H

#include <stdbool.h>

typedef struct vm_entry_command_s vm_entry_command_t;
typedef struct vm_entry_pipeline_s vm_entry_pipeline_t;

static const bool FOREGROUND = true;
static const bool BACKGROUND = false;

void exec_command(vm_entry_command_t *command, int *ret, bool fg);
void exec_pipeline(vm_entry_pipeline_t *pipeline, int *ret, bool fg);

#endif // EXEC_H
