#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../../include/core/shell.h"

int builtin_cd(int argc, char **argv)
{
    char *target = NULL;

    if (argc < 2)
    {
        target = getenv("HOME");
        if (!target)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    else if (strcmp(argv[1], "-") == 0)
    {
        target = getenv("OLDPWD");
        if (!target)
        {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", target);
    }
    else
    {
        target = argv[1];
    }

    char cwd[1024];

    // save current dir → OLDPWD
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        setenv("OLDPWD", cwd, 1);
    }

    if (chdir(target) != 0)
    {
        perror("cd");
        return 1;
    }

    // update PWD
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        setenv("PWD", cwd, 1);
    }

    return 0;
}