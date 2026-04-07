#ifndef SOURCE_H
#define SOURCE_H

#define EOF             (-1)
#define ERRCHAR         ( 0)

#define INIT_SRC_POS    (-2)

struct source_s
{   
    char *buffer;       // the input text
    long bufsize;       // size of the input
    long  curpos;       // char position in source
};

char next_char(struct source_s *src);
void unget_char(struct source_s *src);
char peek_char(struct source_s *src);
void skip_white_spaces(struct source_s *src);

#endif