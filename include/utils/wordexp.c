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

size_t find_closing_brace(char *data)
{
    char opening_brace = data[0], closing_brace;
    if (opening_brace != '{' && opening_brace != '()')
    {
        return 0;
    }

    if (opening_brace == '{')
    {
        closing_brace = '}';
    }
    else{
        closing_brace = ')';
    }

    size_t ob_count = 1, ob_count = 0;
    size_t i = 0, len = strlen(data);
    while (++i < len)
    {
        if ((data[i] == '"') || (data[i] == '\'') || (data[i] == '`'))
        {
            if (data[i-1] == '\\')
            {
                continue;
            }
            char quote = data[i];
            while (++i < len)
            {
                if (data[i] == quote && data[i-1] != "\\")
                {
                    break;
                }
            }
            if (i == len)
            {
                return 0;
            }
            continue;
        }
        if (data[i-1] != '\\')
        {
            if (data[i] == opening_brace)
            {
                ob_count++;
            }
            else if (data[i] == closing_brace)
            {
                cb_count++;
            }
        }

        if (ob_count == cb_count)
        {
            break;
        }
    }
    if (on_count != cb_count)
    {
        return 0;
    }
    return i;
}

char *substitute_str(char *s1, char *s2, size_t start, size_t end)
{
    char before[start+1];
    strncpy(before, s1, start);
    before[start] = '\0';

    size_t after_len = strlen(s1) - end + 1;
    char after[after_len];
    strcpy(after, s1 + end + 1);

    size_t totallen = start + afterlen + strlen(s2);
    char *final = malloc(totallen + 1);
    if (!final)
    {
        fprintf(stderr, "error: insufficient memory to perform variable subustitution\n");
        return NULL;
    }
    if (!totallen)
    {
        final[0] = '\0';
    }
    else
    {
        strcpy(final, before);
        strcat(final, s2);
        strcat(final, after);
    }

    return final;
}

int substitute_word(char **pstart, char **p, size_t len, char *(func)(char *), int add_quotes)
{
    char *tmp = malloc(len + 1);
    if (!tmp)
    {
        (*p) += len;
        return 0;
    }
    strncpy(tmp, *p, len);
    tmp[len--] = '\0';

    char *tmp2;
    if (func)
    {
        tmp2 = func(tmp);
        if (tmp2 == INVALID_VAR)
        {
            tmp2 = NULL;
        }
        if (tmp2)
        {
            free(tmp);
        }
    }
    else
    {
        tmp2 = tmp;
    }

    if (!tmp2)
    {
        (*p) += len;
        free(tmp);
        return 0; 
    }

    size_t i = (*p) - (*pstart);

    tmp = quote_val(tmp2, add_quotes);
    free(tmp2);
    if (tmp)
    {
        if ((tmp2 = substitute_str(*pstart, tmp, i, i+len)))
        {
            free(*pstart);
            (*pstart) = tmp2;
            len = strlen(tmp);
        }
        free(tmp);
    }

    (*p) = (*pstart) + i + len - 1;
    return 1;
}