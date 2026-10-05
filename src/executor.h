#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int apply_redirection(Command *cmd);
int execute_commands(Command *commands);
void reap_background_jobs(void);

#endif
