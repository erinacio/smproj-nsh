#define INCLUDE_VM_INTERNAL
#include <stdio.h>
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "vm_entry.h"
#include "builtin.h"
#include "utils.h"
#include "states.h"
#include "alias.h"
#include "reader.h"

bool is_builtin(vm_entry_command_t *cmd)
{
    if (cmd == NULL || cmd->args == NULL || cmd->args[0] == NULL) return false;
    const char *c = cmd->args[0]->pl_str;

    return (strcmp(c, "cd") == 0 || strcmp(c, "exit") == 0
            || strcmp(c, "alias") == 0 || strcmp(c, "debug") == 0
            || strcmp(c, "export") == 0 || strcmp(c, "unalias") == 0
            || strcmp(c, "history") == 0) || strcmp(c, "unexport") == 0;
}

int call_builtin(vm_entry_command_t *cmd)
{
    if (!is_builtin(cmd)) {
        fputs("nsh: Not a builtin command\n", stderr);
        return -1;
    }
    switch (cmd->args[0]->pl_str[0]) {
    case 'c':
        return builtin_cd(cmd);
    case 'e':
        if (cmd->args[0]->pl_str[2] == 'i') return builtin_exit(cmd);
        return builtin_export(cmd);
    case 'a':
        return builtin_alias(cmd);
    case 'd':
        return builtin_debug(cmd);
    case 'u':
        if (cmd->args[0]->pl_str[2] == 'a') return builtin_unalias(cmd);
        return builtin_unexport(cmd);
    case 'h':
        return builtin_history(cmd);
    }
    fprintf(stderr, "nsh: Unrecognized builtin command `%s'\n", cmd->args[0]->pl_str);
    return -1;
}

#define BUILTIN_ASSERT(x, m) do { if (!(x)) { fputs(m "\n", stderr); return -1; } } while (0)
#define BUILTIN_NORMAL_ASSERT(x) \
    do { \
        BUILTIN_ASSERT(cmd != NULL && cmd->args != NULL, x ": Null parameter"); \
        BUILTIN_ASSERT(cmd->redirs == NULL || cmd->redirs[0] == NULL, x ": Doesn't support io redirection"); \
    } while (0)


int builtin_cd(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("cd");
    BUILTIN_ASSERT(cmd->args[1] == NULL || cmd->args[2] == NULL, "cd: Too many arguments");

    char *path = NULL;
    if (cmd->args[1] == NULL) {
        path = getenv("HOME");
    } else {
        path = cmd->args[1]->pl_str;
    }

    if (path == NULL) path = "/";

    if (chdir(path) == 0) {
        char buff[PATH_MAX + 16];
        if (getcwd(buff, PATH_MAX) != NULL) {
            setenv("PWD", buff, 1);
        }
        return 0;
    } else {
        perror("cd");
        return -1;
    }
}

int builtin_exit(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("exit");
    exit(EXIT_SUCCESS);
    return 0;
}

int builtin_debug(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("debug");
    BUILTIN_ASSERT(cmd->args[1] == NULL, "debug: Too many arguments");
    state_debug = !state_debug;
    printf("debug: now %s\n", state_debug ? "on" : "off");
    return 0;
}

int builtin_alias(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("alias");
    BUILTIN_ASSERT(cmd->args[1] == NULL || cmd->args[2] == NULL, "alias: Too many arguments");
    if (cmd->args[1] == NULL) {
        alias_print_all();
        return 0;
    }
    char *val = cmd->args[1]->pl_str;
    while (*val != '=' && *val != '\0') ++val;
    char *name = cmd->args[1]->pl_str;
    if (*val == '\0') {
        if (!alias_in(name)) return -1;
        printf("%s=%s\n", name, alias_get(name));
        return 0;
    }
    *val++ = '\0';
    if (!alias_add(name, val)) {
        fputs("alias: Add alias failed\n", stderr);
        return -1;
    }
    return 0;
}

extern char **environ;

int builtin_export(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("export");
    BUILTIN_ASSERT(cmd->args[1] == NULL || cmd->args[2] == NULL, "export: Too many arguments");
    if (cmd->args[1] == NULL) {
        for (char **e = environ; *e != NULL; ++e) {
            puts(*e);
        }
        return 0;
    }
    char *val = cmd->args[1]->pl_str;
    while (*val != '=' && *val != '\0') ++val;
    char *name = cmd->args[1]->pl_str;
    if (*val == '\0') {
        char *env = getenv(name);
        if (env != NULL) {
            printf("%s=%s\n", name, env);
            return 0;
        } else {
            fprintf(stderr, "%s is not a environment variable\n", name);
            return -1;
        }
    }
    *val++ = '\0';
    if (setenv(name, val, 1) == -1) {
        perror("nsh: setenv");
        return -1;
    }
    if (strcmp(name, "HISTSIZE") == 0) {
        reader_set_histsize(val);
    }

    return 0;
}

int builtin_unalias(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("unalias");
    BUILTIN_ASSERT(cmd->args[1] != NULL, "unalias: Too few arguments");
    BUILTIN_ASSERT(cmd->args[2] == NULL, "unalias: Too many arguments");
    if (!alias_in(cmd->args[1]->pl_str)) {
        fprintf(stderr, "unalias: `%s' not in alias table\n", cmd->args[1]->pl_str);
        return -1;
    }
    if (!alias_del(cmd->args[1]->pl_str)) {
        fputs("unalias: Failed to remove alias\n", stderr);
        return -1;
    }
    return 0;
}

int builtin_history(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("history");
    BUILTIN_ASSERT(cmd->args[1] == NULL || cmd->args[2] == NULL, "history: Too many arguments");
    int n = 20;
    if (cmd->args[1] != NULL) {
        int ret = sscanf(cmd->args[1]->pl_str, "%d", &n);
        if (ret != 1) {
            fputs("history: Invalid argument\n", stderr);
            return -1;
        }
    }

    HIST_ENTRY **hists = history_list();
    if (hists == NULL) {
        fputs("history: No history\n", stderr);
        return -1;
    }

    int begin = (history_length > n) ? (history_length - n) : 0;
    for (int i = begin; i < history_length; ++i) {
        printf("%8d %s\n", i, hists[i]->line);
    }

    return 0;
}

int builtin_unexport(vm_entry_command_t *cmd)
{
    BUILTIN_NORMAL_ASSERT("unexport");
    BUILTIN_ASSERT(cmd->args[1] != NULL, "unexport: Too few arguments");
    BUILTIN_ASSERT(cmd->args[2] == NULL, "unexport: Too many arguments");
    if (unsetenv(cmd->args[1]->pl_str) == -1) {
        perror("nsh: unsetenv");
        return -1;
    }
    return 0;
}
