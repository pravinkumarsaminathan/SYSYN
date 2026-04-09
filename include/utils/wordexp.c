#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../include/core/shell.h"

struct word_s *make_word(char *str)
{
    /* alloc struct memory */
    struct word_s *word = malloc(sizeof(struct word_s));
    if(!word)
    {
        return NULL;
    }

    /* alloc string memory */
    size_t  len  = strlen(str);
    char   *data = malloc(len+1);
    
    if(!data)
    {
        free(word);
        return NULL;
    }
    
    /* copy string */
    strcpy(data, str);
    word->data = data;
    word->len  = len;
    word->next = NULL;
    
    /* return struct */
    return word;
}


/*
 * free the memory used by a list of words.
 */
void free_all_words(struct word_s *first)
{
    while(first)
    {
        struct word_s *del = first;
        first = first->next;
        
	if(del->data)
        {
            /* free the word text */
            free(del->data);
        }
        
	/* free the word */
        free(del);
    }
}

/*
 * convert a tree of tokens into a command string (i.e. re-create the original
 * command line from the token tree.
 *
 * returns the malloc'd command string, or NULL if there is an error.
 */
char *wordlist_to_str(struct word_s *word)
{
    if(!word)
    {
        return NULL;
    }
    size_t len = 0;
    struct word_s *w = word;
    while(w)
    {
        len += w->len+1;
        w    = w->next;
    }
    char *str = malloc(len+1);
    if(!str)
    {
        return NULL;
    }
    char *str2 = str;
    w = word;
    while(w)
    {
        sprintf(str2, "%s ", w->data);
        str2 += w->len+1;
        w     = w->next;
    }
    /* remove the last separator */
    str2[-1] = '\0';
    return str;
}

void delete_char_at(char *str, size_t index)
{
    char *p1 = str + index;
    char *p2 = p1 + 1;
    while ((*p1++ = *p2++))
    {
        ;
    }
}

int is_name(char *str)
{
    if (!isalpha(*str) && *str != '_')
    {
        return 0;
    }

    while (*++str)
    {
        if (!isalnum(*str) && *str != '_')
        {
            return 0;
        }
    }
    return 1;
}

size_t find_closing_quote(char *data)
{
    char quote = data[0];
    if (quote != '\'' && quote != '"' && quote != '`')
    {
        return 0;
    }

    size_t i = 0, len = strlen(data);
    while (++i < len)
    {
        if (data[i] == quote)
        {
            if (data[i-1] == '\\')
            {
                if (quote != '\'')
                {
                    continue;
                }
            }
            return i;
        }
    }
    return 0;
}