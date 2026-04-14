/* required macro definition for popen() and pclose() */
#define _POSIX_C_SOURCE 200809L

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


/*
 * perform tilde expansion.
 *
 * returns the malloc'd expansion of the tilde prefix, NULL if expansion failed.
 */
char *tilde_expand(char *s)
{
    char *home = NULL;
    size_t len = strlen(s);
    char   *s2 = NULL;
    struct symtab_entry_s *entry;

    /* null tilde prefix. substitute with the value of home */
    if(len == 1)
    {
        entry = get_symtab_entry("HOME");
        if(entry && entry->val)
        {
            home = entry->val;
        }
        else
        {
            /*
             * POSIX doesn't say what to do if $HOME is null/unset.. we follow
             * what bash does, which is searching our home directory in the password
             * database.
             */
            struct passwd *pass;
            pass = getpwuid(getuid());
            if(pass)
            {
                /* get the value of home */
                home = pass->pw_dir;
            }
        }
    }
    else
    {
        /* we have a login name */
        struct passwd *pass;
        pass = getpwnam(s+1);
        if(pass)
        {
            home = pass->pw_dir;
        }
    }

    /* we have a NULL value */
    if(!home)
    {
        return NULL;
    }

    /* return the home dir we've found */
    s2 = malloc(strlen(home)+1);
    if(!s2)
    {
        return NULL;
    }
    strcpy(s2, home);
    return s2;
}


/*
 * perform variable (parameter) expansion.
 * our options are:
 * syntax           POSIX description   var defined     var undefined
 * ======           =================   ===========     =============
 * $var             Substitute          var             nothing
 * ${var}           Substitute          var             nothing
 * ${var:-thing}    Use Deflt Values    var             thing (var unchanged)
 * ${var:=thing}    Assgn Deflt Values  var             thing (var set to thing)
 * ${var:?message}  Error if NULL/Unset var             print message and exit shell,
 *                                                      (if message is empty, print
 *                                                      "var: parameter not set")
 * ${var:+thing}    Use Alt. Value      thing           nothing
 * ${#var}          Calculate String Length
 *
 * Using the same options in the table above, but without the colon, results in
 * a test for a parameter that is unset. using the colon results in a test for a
 * parameter that is unset or null.
 *
 * TODO: we should test our implementation of the following string processing
 *       functions (see section 2.6.2 - Parameter Expansion in POSIX):
 *
 *       ${parameter%[word]}      Remove Smallest Suffix Pattern
 *       ${parameter%%[word]}     Remove Largest Suffix Pattern
 *       ${parameter#[word]}      Remove Smallest Prefix Pattern
 *       ${parameter##[word]}     Remove Largest Prefix Pattern
 */


/*
 * perform variable (parameter) expansion.
 *
 * returns an malloc'd string of the expanded variable value, or NULL if the
 * variable is not defined or the expansion failed.
 *
 * this function should not be called directly by any function outside of this
 * module (hence the double underscores that prefix the function name).
 */
char *var_expand(char *orig_var_name)
{
    /* sanity check */
    if(!orig_var_name)
    {
       return NULL;
    }

    /*
     *  if the var substitution is in the $var format, remove the $.
     *  if it's in the ${var} format, remove the ${}.
     */
    /* skip the $ */
    orig_var_name++;
    size_t len = strlen(orig_var_name);
    if(*orig_var_name == '{')
    {
        /* remove the } */
        orig_var_name[len-1] = '\0';
        orig_var_name++;
    }

    /* check we don't have an empty varname */
    if(!*orig_var_name)
    {
       return NULL;
    }

    int get_length = 0;
    /* if varname starts with #, we need to get the string length */
    if(*orig_var_name == '#')
    {
        /* use of '#' should come with omission of ':' */
        if(strchr(orig_var_name, ':'))
        {
            fprintf(stderr, "error: invalid variable substitution: %s\n", orig_var_name);
            return INVALID_VAR;
        }
        get_length = 1;
        orig_var_name++;
    }

    /* check we don't have an empty varname */
    if(!*orig_var_name)
    {
        return NULL;
    }

    /*
     * search for a colon, which we use to separate the variable name from the
     * value or substitution we are going to perform on the variable.
     */
    char *sub   = strchr(orig_var_name, ':');
    if(!sub)    /* we have a substitution without a colon */
    {
        /* search for the char that indicates what type of substitution we need to do */
        sub = strchr_any(orig_var_name, "-=?+%#");
    }

    /* get the length of the variable name (without the substitution part) */
    len = sub ? (size_t)(sub-orig_var_name) : strlen(orig_var_name);

    /* if we have a colon+substitution, skip the colon */
    if(sub && *sub == ':')
    {
        sub++;
    }

    /* copy the varname to a buffer */
    char var_name[len+1];
    strncpy(var_name, orig_var_name, len);
    var_name[len]   = '\0';

    /*
     * commence variable substitution.
     */
    char *empty_val  = "";
    char *tmp        = NULL;
    char  setme      = 0;

    struct symtab_entry_s *entry = get_symtab_entry(var_name);
    tmp = (entry && entry->val && entry->val[0]) ? entry->val : empty_val;

    /*
     * first case: variable is unset or empty.
     */
    if(!tmp || tmp == empty_val)
    {
        /* do we have a substitution clause? */
        if(sub && *sub)
        {
            /* check the substitution operation we need to perform */
            switch(sub[0])
            {
                case '-':          /* use default value */
                    tmp = sub+1;
                    break;

                case '=':          /* assign the variable a value */
                    /*
                     * NOTE: only variables, not positional or special parameters can be
                     *       assigned this way (we'll fix this later).
                     */
                    tmp = sub+1;
                    /*
                     * assign the EXPANSION OF tmp, not tmp
                     * itself, to var_name (we'll set the value below).
                     */
                    setme = 1;
                    break;

                case '?':          /* print error msg if variable is null/unset */
                    if(sub[1] == '\0')
                    {
                        fprintf(stderr, "error: %s: parameter not set\n", var_name);
                    }
                    else
                    {
                        fprintf(stderr, "error: %s: %s\n", var_name, sub+1);
                    }
                    return INVALID_VAR;

                /* use alternative value (we don't have alt. value here) */
                case '+':
                    return NULL;

                /*
                 * pattern matching notation. can't match anything
                 * if the variable is not defined, now can we?
                 */
                case '#':
                case '%':
                    break;

                default:                /* unknown operator */
                    return INVALID_VAR;
            }
        }
        /* no substitution clause. return NULL as the variable is unset/null */
        else
        {
            tmp = empty_val;
        }
    }
    /*
     * second case: variable is set/not empty.
     */
    else
    {
        /* do we have a substitution clause? */
        if(sub && *sub)
        {
            /* check the substitution operation we need to perform */
            switch(sub[0])
            {
                case '-':          /* use default value */
                case '=':          /* assign the variable a value */
                case '?':          /* print error msg if variable is null/unset */
                    break;

                /* use alternative value */
                case '+':
                    tmp = sub+1;
                    break;

                /*
                 * for the prefix and suffix matching routines (below).
                 * bash expands the pattern part, but ksh doesn't seem to do
                 * the same (as far as the manpage is concerned). we follow ksh.
                 */
                case '%':       /* match suffix */
                    sub++;
                    /* perform word expansion on the value */
                    char *p = word_expand_to_str(tmp);
                    /* word expansion failed */
                    if(!p)
                    {
                        return INVALID_VAR;
                    }
                    int longest = 0;
                    /* match the longest or shortest suffix */
                    if(*sub == '%')
                    {
                        longest = 1, sub++;
                    }
                    /* perform the match */
                    if((len = match_suffix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    /* return the match */
                    char *p2 = malloc(len+1);
                    if(p2)
                    {
                        strncpy(p2, p, len);
                        p2[len] = '\0';
                    }
                    free(p);
                    return p2;

                case '#':       /* match prefix */
                    sub++;
                    /* perform word expansion on the value */
                    p = word_expand_to_str(tmp);
                    /* word expansion failed */
                    if(!p)
                    {
                        return INVALID_VAR;
                    }
                    longest = 0;
                    /* match the longest or shortest suffix */
                    if(*sub == '#')
                    {
                        longest = 1, sub++;
                    }
                    /* perform the match */
                    if((len = match_prefix(sub, p, longest)) == 0)
                    {
                        return p;
                    }
                    /* return the match */
                    p2 = malloc(strlen(p)-len+1);
                    if(p2)
                    {
                        strcpy(p2, p+len);
                    }
                    free(p);
                    return p2;

                default:                /* unknown operator */
                    return INVALID_VAR;
            }
        }
        /* no substitution clause. return the variable's original value */
    }

    /*
     * we have substituted the variable's value. now go POSIX style on it.
     */
    int expanded = 0;
    if(tmp)
    {
        if((tmp = word_expand_to_str(tmp)))
        {
            expanded = 1;
        }
    }

    /* do we need to set new value to the variable? */
    if(setme)
    {
        /* if variable not defined, add it now */
        if(!entry)
        {
            entry = add_to_symtab(var_name);
        }
        /* and set its value */
        if(entry)
        {
            symtab_entry_setval(entry, tmp);
        }
    }

    char buf[32];
    char *p = NULL;
    if(get_length)
    {
        if(!tmp)
        {
            sprintf(buf, "0");
        }
        else
        {
            sprintf(buf, "%lu", strlen(tmp));
        }
        /* get a copy of the buffer */
        p = malloc(strlen(buf)+1);
        if(p)
        {
            strcpy(p, buf);
        }
    }
    else
    {
        /* "normal" variable value */
        p = malloc(strlen(tmp)+1);
        if(p)
        {
            strcpy(p, tmp);
        }
    }

    /* free the expanded word list */
    if(expanded)
    {
        free(tmp);
    }

    /* return the result */
    return p ? : INVALID_VAR;
}


/*
 * perform command substitutions.
 * the backquoted flag tells if we are called from a backquoted command substitution:
 *
 *    `command`
 *
 * or a regular one:
 *
 *    $(command)
 */
char *command_substitute(char *orig_cmd)
{
    char    b[1024];
    size_t  bufsz = 0;
    char   *buf   = NULL;
    char   *p     = NULL;
    int     i     = 0;
    int backquoted = (*orig_cmd == '`');

    /*
     * fix cmd in the backquoted version.. we skip the first char (if using the
     * old, backquoted version), or the first two chars (if using the POSIX version).
     */
    char *cmd = malloc(strlen(orig_cmd+1));
    
    if(!cmd)
    {
        fprintf(stderr, "error: insufficient memory to perform command substitution\n");
        return NULL;
    }
    
    strcpy(cmd, orig_cmd+(backquoted ? 1 : 2));
    
    char *cmd2 = cmd;
    size_t cmdlen = strlen(cmd);
    
    if(backquoted)
    {
        /* remove the last back quote */
        if(cmd[cmdlen-1] == '`')
        {
            cmd[cmdlen-1] = '\0';
        }
        
	/* fix the backslash-escaped chars */
        char *p1 = cmd;
        
	do
        {
            if(*p1 == '\\' &&
               (p1[1] == '$' || p1[1] == '`' || p1[1] == '\\'))
            {
                char *p2 = p1, *p3 = p1+1;
                while((*p2++ = *p3++))
                {
                    ;
                }
            }
        } while(*(++p1));
    }
    else
    {
        /* remove the last closing brace */
        if(cmd[cmdlen-1] == ')')
        {
            cmd[cmdlen-1] = '\0';
        }
    }

    FILE *fp = popen(cmd2, "r");

    /* check if we have opened the pipe */
    if(!fp)
    {
        free(cmd2);
        fprintf(stderr, "error: failed to open pipe: %s\n", strerror(errno));
        return NULL;
    }

    /* read the command output */
    while((i = fread(b, 1, 1024, fp)))
    {
        /* first time. alloc buffer */
        if(!buf)
        {
            /* add 1 for the null terminating byte */
            buf = malloc(i+1);
            if(!buf)
            {
                goto fin;
            }
            
	    p   = buf;
        }
        /* extend buffer */
        else
        {
            char *buf2 = realloc(buf, bufsz+i+1);
            
	    if(!buf2)
            {
                free(buf);
                buf = NULL;
                goto fin;
            }
            
	    buf = buf2;
            p   = buf+bufsz;
        }
        
	bufsz += i;
        
	/* copy the input and add the null terminating byte */
        memcpy(p, b, i);
        p[i] = '\0';
    }
    
    if(!bufsz)
    {
        free(cmd2);
        return NULL;
    }
    
    /* now remove any trailing newlines */
    i = bufsz-1;
    
    while(buf[i] == '\n' || buf[i] == '\r')
    {
        buf[i] = '\0';
        i--;
    }

fin:
    /* close the pipe */
    pclose(fp);

    /* free used memory */
    free(cmd2);
    
    if(!buf)
    {
        fprintf(stderr, "error: insufficient memory to perform command substitution\n");
    }
    
    return buf;
}