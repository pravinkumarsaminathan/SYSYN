#ifndef SYSTAB_H
#define SYSTAB_H

#include "../parser/node.h"

#define MAX_SYMTAB	256

// the type of a symbol table entry's value
enum symbol_type_e
{
    SYM_STR ,
    SYM_FUNC,
};

// the symbol table entry structure
struct symtab_entry_s
{
    char     *name;
    enum      symbol_type_e val_type;
    char     *val;
    unsigned  int flags;
    struct    symtab_entry_s *next;
    struct    node_s *func_body;
};

// the symbol table structure
struct symtab_s
{
    int    level;
    struct symtab_entry_s *first, *last;
};

// values for the flags field of struct symtab_entry_s                     
#define FLAG_EXPORT (1 << 0) // export entry to forked commands

// the symbol table stack structure
struct symtab_stack_s
{
    int    symtab_count;
    struct symtab_s *symtab_list[MAX_SYMTAB];
    struct symtab_s *global_symtab, *local_symtab;
};


struct symtab_s       *new_symtab(int level);
struct symtab_s       *symtab_stack_push(void);
struct symtab_s       *symtab_stack_pop(void);
int rem_from_symtab(struct symtab_entry_s *entry, struct symtab_s *symtab);
struct symtab_entry_s *add_to_symtab(char *symbol);
struct symtab_entry_s *do_lookup(char *str, struct symtab_s *symtable);
struct symtab_entry_s *get_symtab_entry(char *str);
struct symtab_s       *get_local_symtab(void);
struct symtab_s       *get_global_symtab(void);
struct symtab_stack_s *get_symtab_stack(void);
void init_symtab(void);
void dump_local_symtab(void);
void free_symtab(struct symtab_s *symtab);
void symtab_entry_setval(struct symtab_entry_s *entry, char *val); 

#endif