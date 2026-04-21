#include "../../include/core/shell.h"

struct builtin_s builtins[] =
{   
    {"cd"       , builtin_cd },
    { "dump"    , dump       },
};

int builtins_count = sizeof(builtins)/sizeof(struct builtin_s);