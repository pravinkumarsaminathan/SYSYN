#include "../../include/core/shell.h"
#include "../../include/utils/symtab.h"

int dump(int argc, char **argv)
{
    dump_local_symtab();
    return 0;
}