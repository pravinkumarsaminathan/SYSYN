#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <glob.h>

void print_prompt1(void);
void print_prompt2(void);

char *read_cmd(void);

#include "source.h"
int  parse_and_execute(struct source_s *src);

void initsh(void);

// shell builtin utilities
int dump(int argc, char **argv);

// struct for builtin utilities
struct builtin_s
{
    char *name;    // utility name
    int (*func)(int argc, char **argv); // function to call to execute the utility
};

// the list of builtin utilities
extern struct builtin_s builtins[];

// and their count
extern int builtins_count;

struct word_s
{
    char *data;
    int len;
    struct word_s *next;
};

struct word_s *make_word(char *str);
void free_all_words(struct word_s *first);

size_t  find_closing_quote(char *data);
size_t  find_closing_brace(char *data);
void    delete_char_at(char *str, size_t index);
char   *substitute_str(char *s1, char *s2, size_t start, size_t end);
char   *wordlist_to_str(struct word_s *word);

/* some string manipulation functions */
char   *strchr_any(char *string, char *chars);
char   *quote_val(char *val, int add_quotes);
int     check_buffer_bounds(int *count, int *len, char ***buf);
void    free_buffer(int len, char **buf);
#endif