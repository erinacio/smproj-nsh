#ifndef READER__H
#define READER__H

char *reader_readline();
char *reader_readmore();
void reader_addhist(const char *line);
char **reader_completion(const char *text, int start, int end);
char *reader_expand_history(char *line);
void reader_load_history();
void reader_save_history();

void reader_set_histsize(const char *v);

#endif // READER__H
