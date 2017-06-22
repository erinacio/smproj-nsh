#include <stdio.h>
#include <string.h>
#include <errno.h>
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

void panic(const char *reason)
{
    fprintf(stderr, "PANIC: %s\n", reason);
    fprintf(stderr, "       errno = %d\n", errno);
    fprintf(stderr, "       strerror(errno) = %s\n", strerror(errno));
    fflush(stdout);
    abort();
}

char *str_join(const char *init, const char *sep, const char *follow)
{
    if (init == NULL || follow == NULL) return NULL;
    ptrdiff_t init_len = strlen(init), follow_len = strlen(follow);
    ptrdiff_t sep_len = (sep == NULL) ? 0 : strlen(sep);
    ptrdiff_t new_len = init_len + follow_len + sep_len;
    if (new_len < 0) return NULL;

    char *new_str = malloc(new_len + 1);
    if (new_str == NULL) return NULL;

    memcpy(new_str, init, init_len);
    if (sep != NULL) memcpy(new_str + init_len, sep, sep_len);
    memcpy(new_str + init_len + sep_len, follow, follow_len);
    new_str[new_len] = '\0';

    return new_str;
}

void print_char_repr(int ch)
{
    if (isprint(ch) && ch != '\'' && ch != '"' && ch != '\\') {
        putchar(ch);
    } else if (ch == '\'' || ch == '"' || ch == '\\') {
        printf("\\%c", ch);
    } else if (ch != -1) {
        printf("\\x%02x", (ch < 0 ? 0x100 + ch : ch));
    } else {
        printf("\\#");
    }
}

void print_str_repr(const char *str, int max_len)
{
    if (str == NULL) printf("(null)");
    if (max_len < 0) max_len = INT_MAX;
    putchar('"');
    for (int i = 0; *str != '\0' && i < max_len; ++str, ++i) {
        print_char_repr(*str);
    }
    putchar('"');
}

void init_env()
{
    char buff[PATH_MAX * 2];

    // SHELL
    if (readlink("/proc/self/exe", buff, PATH_MAX) != -1) {
        setenv("SHELL", buff, 1);
    }

    // PATH
    if (getenv("PATH") == NULL) {
        setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
    }

    if (getcwd(buff, PATH_MAX) != NULL) {
        setenv("PWD", buff, 1);
    }
}

char *tilde_expand(const char *s)
{
    if (s == NULL) return NULL;
    if (s[0] != '~') return NULL;
    char *str = strdup(s);
    if (str == NULL) return NULL;
    char *path_begin = str + 1;
    if (*path_begin != '/') {
        ++path_begin;
        while (*path_begin != '\0' && *path_begin != '/') ++path_begin;
        if (*path_begin != '\0') {
            *path_begin++ = '\0';
        }
    }
    struct passwd *pw = NULL;
    if (str[1] == '/' || str[1] == '\0') {
        pw = getpwuid(getuid());
        if (pw == NULL) perror("nsh: getpwuid");
    } else {
        pw = getpwnam(str + 1);
        if (pw == NULL) perror("nsh: getpwnam");
    }
    if (pw == NULL) { free(str); return NULL; }

    const char *homedir = pw->pw_dir;
    if (homedir == NULL) { free(str); return NULL; }

    char *new_str = malloc(strlen(homedir) + strlen(path_begin) + 2);
    if (new_str == NULL) { free(str); return NULL; }
    strcpy(new_str, homedir);
    if (*path_begin != '/') {
        strcat(new_str, "/");
    }
    strcat(new_str, path_begin);

    free(str);
    return new_str;
}
