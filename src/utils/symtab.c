#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../include/core/shell.h"
#include "../../include/parser/node.h"
#include "../../include/parser/parser.h"
#include "../../include/utils/symtab.h"

struct symtab_stack_s symtab_stack;
int symtab_level;

void init_symtab(void)
{
    symtab_stack.symtab_count = 1;
    symtab_level = 0;

    struct symtab_s *global_symtab = malloc(sizeof(struct symtab_s));

    if (!global_symtab)
    {
        fprintf(stderr, "fatal error: no memory for global symbol table\n");
        exit(EXIT_FAILURE);
    }

    memset(global_symtab, 0, sizeof(struct symtab_s));
    symtab_stack.global_symtab = global_symtab;
    symtab_stack.local_symtab = global_symtab;
    symtab_stack.symtab_list[0] = global_symtab;
    global_symtab->level = 0;
}

struct symtab_s *new_symtab(int level)
{
    struct symtab_s *symtab = malloc(sizeof(struct symtab_s));

    if (!symtab)
    {
        fprintf(stderr, "fatel error : no memory for new symbol table\n");
        exit(EXIT_FAILURE);
    }

    memset(symtab, 0, sizeof(struct symtab_s));
    symtab_level = level;
    return symtab;
}

void free_symtab(struct symtab_s *symtab)
{
    if (!symtab)
    {
        return;
    }

    struct symtab_entry_s *entry = symtab->first;
    
    while (entry)
    {
        if (entry->name)
        {
            free(entry->name);
        }
        
        if (entry->val)
        {
            free(entry_>val);
        }

        if (entry->func_body)
        {
            free_node_tree(entry->func_body);
        }

        struct symtab_entry_s *next = entry->next;
        free(entry);
        entry = next;
    }
    
    free(symtab);
}

void dump_local_symtab(void)
{
    struct symtab_s *symtab = symtab_stack.local_symtab;
    int i = 0;
    int indent = symtab->level * 4;

    fprintf(stderr, "%*sSymbol Table [Level %d]:\r\n", indent, " ", symtab->level);
    fprintf(stderr, "%*s===========================\r\n", indent, " ");
    fprintf(stderr, "%*s  No               Symbol                    Val\r\n", indent, " ");
    fprintf(stderr, "%*s----- --------------------------------- ------------\r\n", indent, " ");

    struct symtab_entry_s *entry = symtab->first;

    while (entry)
    {
        fprintf(stderr, "%*s[%04d] %-32s '%s'\r\n", indent, " ", i++, entry->name, entry->val);
        entry = entry->next;
    }

    fprintf(stderr, "%*s----- --------------------------------- ------------\r\n", indent, " ");
}

struct symtab_entry_s *add_to_symtab(char *symbol)
{
    if (!symbol || symbol[0] == '\0')
    {
        return NULL;
    }

    struct symtab_s *st = symtab_stack.local_symtab;
    struct symtab_entry_s *entry = NULL;

    if ((entry = do_lookup((symbol, st))))
    {
        return entry;
    }

    entry = malloc(sizeof(struct symtab_entry_s));

    if (!entry)
    {
        fprintf(stderr, "fatal error : no memory for new symbol table entry\n");
        exit(EXIT_FAILURE);
    }

    memset(entry, 0, sizeof(struct symtab_entry_s));
    entry->name = malloc(strlen(symbol) + 1);

    if (!entry->name)
    {
        fprintf(stderr, "fatal error : no memory for new symbol table entry\n");
        exit(EXIT_FAILURE);
    }

    strcpy(entry->name, symbol);

    if (!st->fisrt)
    {
        st->first = entry;
        st->last = entry;
    }
    else
    {
        st->last->next = entry;
        st->last = entry;
    }

    return entry;

}

int rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab)
{
    int res = 0;
    if (entry->value)
    {
        free(entry->value);
    }

    if (entry->func_body)
    {
        free_node_tree(entry->func_body);
    }

    free(entry->name);

    if (symtab->first == entry)
    {
        symtab->first = symtab->first->next;

        if (symtab->last == entry)
        {
            symtab->last = NULL;
        }
        res = 1;
    }
    else
    {
        struct symtab_entry_s *e = symtab->first;
        struct symtab_entry_s *p = NULL;

        while (e && e != entry)
        {
            p = e;
            e = e->next;
        }

        if (e == entry)
        {
            p->next = e->next;
            res = 1;
        }
    }

    free(entry);
    return res;
}

struct symtab_entry_s *do_lookup(char *str, struct symtab_s *symtable)
{
    if (!str || !symtable)
    {
        return NULL;
    }

    struct symtab_entry_s *entry = symtable->first;

    while (entry)
    {
        if (strcmp(entry->name, str) == 0)
        {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

struct symtab_entry_s *get_symtab_entry(char *str)
{
    int i = symtab_stack.symtab_count - 1;

    do
    {
        struct symtab_s *symtab = symtab_stack.symtab_list[i];
        struct symtab_entry_s *entry = do_lookup(str, symtab);

        if (entry)
        {
            return entry;
        }
    } while (--i >= 0);

    return NULL;
}

void symtab_entry_setval(struct symtab_entry_s *entry, char *val)
{
    if (entry->val)
    {
        free(entry->val);
    }

    if (!val)
    {
        entry->val = NULL;
    }
    else
    {
        char *val2 = malloc(strlen(val) + 1);

        if (val2)
        {
            strcpy(val2, val);
        }
        else
        {
            fprintf(stderr, "error : no memory for symbol table entry's value\n");
        }

        entry->val = val2;
    }
}