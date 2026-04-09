#include "../../include/core/shell.h"

struct builtin_s builtins[] =
{   
    { "dump"    , dump       },
};

int builtins_count = sizeof(builtins)/sizeof(struct builtin_s);