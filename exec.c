#define INCLUDE_VM_INTERNAL
#include <stdio.h>
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "vm_entry.h"
#include "exec.h"
#include "builtin.h"
#include "utils.h"

static void exec_external(vm_entry_command_t *command)
{
    int argc = 0;
    while(command->args[argc] != NULL) ++argc;
    if (argc == 0) {
        fputs("nsh: no command", stderr);
        exit(EXIT_FAILURE);
    }

    // argv
    char **argv = malloc(sizeof(char *) * (argc + 1));
    if (argv == NULL) panic("nsh: malloc failed");
    for (int i = 0; i < argc; ++i) {
        argv[i] = command->args[i]->pl_str;
    }
    argv[argc] = NULL;

    // env
    for (vm_entry_assign_t **pa = command->assigns; *pa != NULL; ++pa) {
        setenv((*pa)->pl_name, (*pa)->pl_val, 1);
    }

    // io redir
    int new_fd;
    for (vm_entry_ioredir_t **pr = command->redirs; *pr != NULL; ++pr) {
        vm_entry_ioredir_t *e = *pr;
        switch (e->redir_type) {
        case IO_REDIR_INPUT:
            if ((new_fd = openat(AT_FDCWD, e->pl_path, O_RDONLY)) < 0) perror("nsh: openat");
            if (dup2(new_fd, e->pl_fd) < 0) perror("nsh: dup2");
            if (new_fd >= 0) close(new_fd);
            break;
        case IO_REDIR_OUTPUT:
        case IO_REDIR_OUTPUT_CLOBBER:
            if ((new_fd = openat(AT_FDCWD, e->pl_path, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) perror("nsh: openat");
            if (dup2(new_fd, e->pl_fd) < 0) perror("nsh: dup2");
            if (new_fd >= 0) close(new_fd);
            break;
        case IO_REDIR_OUTPUT_APPEND:
            if ((new_fd = openat(AT_FDCWD, e->pl_path, O_WRONLY | O_APPEND | O_CREAT, 0644) < 0)) perror("nsh: openat");
            if (dup2(new_fd, e->pl_fd) < 0) perror("nsh: dup2");
            if (new_fd >= 0) close(new_fd);
            break;
        case IO_REDIR_INPUT_DUP:
        case IO_REDIR_OUTPUT_DUP:
            if (dup2(e->pl_fd2, e->pl_fd) < 0) perror("nsh: dup2");
            break;
        case IO_REDIR_INOUT:
            if ((new_fd = openat(AT_FDCWD, e->pl_path, O_RDWR | O_CREAT, 0644) < 0)) perror("nsh: openat");
            if (dup2(new_fd, e->pl_fd) < 0) perror("nsh: dup2");
            if (new_fd >= 0) close(new_fd);
            break;
        default:
            break;
        }
    }

    // pipe
    if (command->pipe_in >= 0) {
        if (dup2(command->pipe_in, STDIN_FILENO) < 0) perror("nsh: dup2");
        close(command->pipe_in);
    }
    if (command->pipe_out >= 0) {
        if (dup2(command->pipe_out, STDOUT_FILENO) < 0) perror("nsh: dup2");
        close(command->pipe_out);
    }

    execvp(command->args[0]->pl_str, argv);
    perror("nsh");
    exit(EXIT_FAILURE);
}

void exec_command(vm_entry_command_t *command, int *ret, bool fg)
{
    int tmp = 0;
    if (ret == NULL) ret = &tmp;
    if (is_builtin(command)) {
        if (!fg) { fputs("nsh: Can't put builtin commands into background\n", stderr); return; }
        *ret = call_builtin(command);
        return;
    }
    if (command->args[0] == NULL) {
        fputs("nsh: Empty commands not allowed here", stderr);
        return;
    }

    pid_t pid;
    if ((pid = fork()) == 0) {
        if (!fg) { if (fork() != 0) exit(EXIT_SUCCESS); }
        command->pipe_in = command->pipe_out = -1;
        exec_external(command);
        exit(EXIT_FAILURE);
    } else {
        if (waitpid(pid, ret, 0) == -1) perror("nsh: waitpid");
    }
}

void exec_pipeline(vm_entry_pipeline_t *pipeline, int *ret, bool fg)
{
    int tmp = 0;
    if (ret == NULL) ret = &tmp;
    pid_t pid;
    int fds[2];

    if (pipeline == NULL || pipeline->commands[0] == NULL) return;
    if (pipeline->commands[1] == NULL) exec_command(pipeline->commands[0], ret, fg);

    for (vm_entry_command_t **pe = pipeline->commands; *pe != NULL; ++pe) {
        vm_entry_command_t *e = *pe;
        if (is_builtin(e)) {
            fputs("nsh: Builtin commands not allowed in pipelines", stderr);
            return;
        }
        if (e->args[0] == NULL) {
            fputs("nsh: Empty commands not allowed in pipelines", stderr);
            return;
        }
    }

    if ((pid = fork()) == 0) {
        if (!fg) { if (fork() != 0) exit(EXIT_SUCCESS); }

        vm_entry_command_t *left = pipeline->commands[0], *right = pipeline->commands[1];
        for (int i = 1; right != NULL; right = pipeline->commands[++i]) {
            if (pipe(fds) == -1) {
                perror("nsh: pipe");
                exit(EXIT_FAILURE);
            }
            left->pipe_out = fds[1];
            right->pipe_in = fds[0];
            left = right;
        }

        pid_t last_pid = -1;
        for (vm_entry_command_t **pe = pipeline->commands; *pe != NULL; ++pe) {
            vm_entry_command_t *e = *pe;
            if ((last_pid = fork()) == 0) {
                exec_external(e);
                exit(EXIT_FAILURE);
            } else {
                if (e->pipe_in >= 0) close(e->pipe_in);
                if (e->pipe_out >= 0) close(e->pipe_out);
            }
        }

        int status, last_ret = EXIT_FAILURE;
        pid_t wpid;
        while ((wpid = wait(&status)) > 0) {
            if (wpid != last_pid) continue;
            if (WIFEXITED(status)) last_ret = WEXITSTATUS(status);
        }
        exit(last_ret);

    } else {
        if (waitpid(pid, ret, 0) == -1) perror("nsh: waitpid");
    }
}
