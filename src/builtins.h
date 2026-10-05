#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int is_builtin(const char *name);
int run_builtin(const Command *command, int *should_exit);
void print_help(void);

#endif
