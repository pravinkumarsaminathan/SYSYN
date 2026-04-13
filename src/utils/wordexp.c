#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include "../../include/core/shell.h"
#include "../../include/utils/symtab.h"
#include "../../include/executor/executor.h"

/* special value to represent an invalid variable */
#define INVALID_VAR     ((char *)-1)


/*
 * convert the string *word to a cmd_token struct, so it can be passed to
 * functions such as word_expand().
 *
 * returns the malloc'd cmd_token struct, or NULL if insufficient memory.
 */
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


/*
 * delete the character at the given index in the given str.
 */
void delete_char_at(char *str, size_t index)
{
    char *p1 = str+index;
    char *p2 = p1+1;
    while((*p1++ = *p2++))
    {
        ;
    }
}


/*
 * check if the given str is a valid name.. POSIX says a names can consist of
 * alphanumeric chars and underscores, and start with an alphabetic char or underscore.
 *
 * returns 1 if str is a valid name, 0 otherwise.
 */
int is_name(char *str)
{
    /* names start with alpha char or an underscore... */
    if(!isalpha(*str) && *str != '_')
    {
        return 0;
    }
    /* ...and contain alphanumeric chars and/or underscores */
    while(*++str)
    {
        if(!isalnum(*str) && *str != '_')
        {
            return 0;
        }
    }
    return 1;
}


/*
 * find the closing quote that matches the opening quote, which is the first
 * char of the data string.
 * sq_nesting is a flag telling us if we should allow single quote nesting
 * (prohibited by POSIX, but allowed in ANSI-C strings).
 *
 * returns the zero-based index of the closing quote.. a return value of 0
 * means we didn't find the closing quote.
 */
size_t find_closing_quote(char *data)
{
    /* check the type of quote we have */
    char quote = data[0];
    if(quote != '\'' && quote != '"' && quote != '`')
    {
        return 0;
    }
    /* find the matching closing quote */
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if(data[i] == quote)
        {
            if(data[i-1] == '\\')
            {
                if(quote != '\'')
                {
                    continue;
                }
            }
            return i;
        }
    }
    return 0;
}


/*
 * find the closing brace that matches the opening brace, which is the first
 * char of the data string.
 *
 * returns the zero-based index of the closing brace.. a return value of 0
 * means we didn't find the closing brace.
 */
size_t find_closing_brace(char *data)
{
    /* check the type of opening brace we have */
    char opening_brace = data[0], closing_brace;
    if(opening_brace != '{' && opening_brace != '(')
    {
        return 0;
    }
    /* determine the closing brace according to the opening brace */
    if(opening_brace == '{')
    {
        closing_brace = '}';
    }
    else
    {
        closing_brace = ')';
    }
    /* find the matching closing brace */
    size_t ob_count = 1, cb_count = 0;
    size_t i = 0, len = strlen(data);
    while(++i < len)
    {
        if((data[i] == '"') || (data[i] == '\'') || (data[i] == '`'))
        {
            /* skip escaped quotes */
            if(data[i-1] == '\\')
            {
                continue;
            }
            /* skip quoted substrings */
            char quote = data[i];
            while(++i < len)
            {
                if(data[i] == quote && data[i-1] != '\\')
                {
                    break;
                }
            }
            if(i == len)
            {
                return 0;
            }
            continue;
        }
        /* keep the count of opening and closing braces */
        if(data[i-1] != '\\')
        {
            if(data[i] == opening_brace)
            {
                ob_count++;
            }
            else if(data[i] == closing_brace)
            {
                cb_count++;
            }
        }
        /* break when we have a matching number of opening and closing braces */
        if(ob_count == cb_count)
        {
            break;
        }
    }
    if(ob_count != cb_count)
    {
        return 0;
    }
    return i;
}


/*
 * substitute the substring of s1, from character start to character end,
 * with the s2 string.
 *
 * start should point to the first char to be deleted from s1.
 * end should point to the last char to be deleted from s, NOT the
 * char coming after it.
 *
 * returns the malloc'd new string, or NULL on error.
 */
char *substitute_str(char *s1, char *s2, size_t start, size_t end)
{
    /* get the prefix (the part before start) */
    char before[start+1];
    strncpy(before, s1, start);
    before[start]   = '\0';
    /* get the postfix (the part after end) */
    size_t afterlen = strlen(s1)-end+1;
    char after[afterlen];
    strcpy(after, s1+end+1);
    /* alloc memory for the new string */
    size_t totallen = start+afterlen+strlen(s2);
    char *final = malloc(totallen+1);
    if(!final)
    {
        fprintf(stderr, "error: insufficient memory to perform variable substitution\n");
        return NULL;
    }
    if(!totallen)       /* empty string */
    {
        final[0] = '\0';
    }
    else                /* concatenate the three parts into one string */
    {
        strcpy(final, before);
        strcat(final, s2    );
        strcat(final, after );
    }
    /* return the new string */
    return final;
}


int substitute_word(char **pstart, char **p, size_t len,
                    char *(func)(char *),
                    int add_quotes)
{
    /* extract the word to be substituted */
    char *tmp = malloc(len+1);
    if(!tmp)
    {
        (*p) += len;
        return 0;
    }
    strncpy(tmp, *p, len);
    tmp[len--] = '\0';

    /* and expand it */
    char *tmp2;
    if(func)
    {
        tmp2 = func(tmp);
        if(tmp2 == INVALID_VAR)
        {
            tmp2 = NULL;
        }
        if(tmp2)
        {
            free(tmp);
        }
    }
    else
    {
        tmp2 = tmp;
    }

    /* error expanding the string. keep the original string as-is */
    if(!tmp2)
    {
        (*p) += len;
        free(tmp);
        return 0;
    }

    /* save our current position in the word */
    size_t i = (*p)-(*pstart);

    /* substitute the expanded word */
    tmp = quote_val(tmp2, add_quotes);
    free(tmp2);
    if(tmp)
    {
        /* substitute the expanded word */
        if((tmp2 = substitute_str(*pstart, tmp, i, i+len)))
        {
            /* adjust our pointer to point to the new string */
            free(*pstart);
            (*pstart) = tmp2;
            len = strlen(tmp);
        }
        free(tmp);
    }

    /* adjust our pointer to point to the new string */
    (*p) = (*pstart)+i+len-1;
    return 1;
}