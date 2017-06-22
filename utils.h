#ifndef UTILS_H
#define UTILS_H

void panic(const char *reason);

char *str_join(const char *init, const char *sep, const char *follow);

void print_char_repr(int ch);
void print_str_repr(const char *str, int max_len);

void init_env(void);

char *tilde_expand(const char *str);

#define UNUSED_VAR(x) (void)(x)

#define EXPLICIT_FALLTHROUGH __attribute__((fallthrough))

#endif // UTILS_H
