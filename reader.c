#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>
#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include "states.h"
#include "utils.h"

char *reader_readline()
{
    char buff[PATH_MAX];
    memset(buff, 0, PATH_MAX);
    printf("\n\033[93m%s\033[0m%s\n", getcwd(buff, PATH_MAX), (state_debug ? " \033[91mDEBUG\033[0m" : ""));
    return readline((getuid() != 0) ? "$ " : "# ");
}

char *reader_readmore()
{
    return readline("> ");
}

void reader_addhist(const char *line)
{
    add_history(line);
}

static char *file_name_generator(const char *text, int state)
{
    static DIR *dp;
    static int len;

    if (!state) {
        dp = opendir(".");
        len = strlen(text);
    }

    if (dp == NULL) return NULL;

    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (strncmp(ent->d_name, text, len) == 0) {
            return strdup(ent->d_name);
        }
    }

    return NULL;
}

char **reader_completion(const char *text, int start, int end)
{
    UNUSED_VAR(start); UNUSED_VAR(end);
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, file_name_generator);
}

char *reader_expand_history(char *line)
{
    char *output;
    switch (history_expand(line, &output)) {
    case 0:
    case -1:
        return line;
    case 1:
        free(line);
        puts(output);
        return output;
    case 2:
        puts(output);
        free(output);
        return line;
    default:
        return line;
    }
}

void reader_load_history()
{
    char *path = tilde_expand("~/.nsh_history");
    if (path == NULL) return;
    int err = read_history(path);
    if (err != 0) {
        printf("nsh: read_history: %s\n", strerror(err));
    }
}

void reader_save_history()
{
    char *path = tilde_expand("~/.nsh_history");
    if (path == NULL) return;
    int err = write_history(path);
    if (err != 0) {
        printf("nsh: write_history: %s\n", strerror(err));
    }
}

void reader_set_histsize(const char *v)
{
    if (v == NULL) v = getenv("HISTSIZE");
    if (v == NULL || *v == '\0') {
        unstifle_history();
        return;
    }

    int histsize = 0;
    int ret = sscanf(v, "%d", &histsize);
    if (ret != 1 || histsize < 0) return;
    stifle_history(histsize);
}
