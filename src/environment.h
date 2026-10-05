#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

char *resolve_executable_path(const char *cmd);
char *shell_getenv(const char *name);

#endif
