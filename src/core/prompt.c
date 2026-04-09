#include <stdio.h>
#include "../../include/core/shell.h"
#include "../../include/utils/symtab.h"

void print_prompt1(void)
{
    struct symtab_entry_s *entry = get_symtab_entry("PS1");

    if(entry && entry->val)
    {
        fprintf(stderr, "%s", entry->val);
    }
    else
    {
        fprintf(stderr, "$ ");
    }
}


void print_prompt2(void)
{
    struct symtab_entry_s *entry = get_symtab_entry("PS2");

    if(entry && entry->val)
    {
        fprintf(stderr, "%s", entry->val);
    }
    else
    {
        fprintf(stderr, "> ");
    }
}