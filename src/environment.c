#include "environment.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *shell_getenv(const char *name)
{
    return getenv(name);
}

char *resolve_executable_path(const char *cmd)
{
    const char *path;
    const char *entry;
    const char *separator;

    if (cmd == NULL || *cmd == '\0')
        return NULL;
    if (strchr(cmd, '/') != NULL)
        return strdup(cmd);

    path = shell_getenv("PATH");
    if (path == NULL)
        return NULL;

    entry = path;
    for (;;) {
        size_t directory_length;
        size_t command_length = strlen(cmd);
        size_t full_length;
        char *candidate;

        separator = strchr(entry, ':');
        directory_length = separator == NULL
                               ? strlen(entry)
                               : (size_t)(separator - entry);
        full_length = directory_length + (directory_length == 0 ? 0 : 1) +
                      command_length + 1;
        candidate = malloc(full_length);
        if (candidate == NULL)
            return NULL;
        if (directory_length > 0) {
            memcpy(candidate, entry, directory_length);
            candidate[directory_length] = '/';
            memcpy(candidate + directory_length + 1, cmd, command_length + 1);
        } else {
            memcpy(candidate, cmd, command_length + 1);
        }
        if (access(candidate, X_OK) == 0)
            return candidate;
        free(candidate);
        if (separator == NULL)
            break;
        entry = separator + 1;
    }
    return NULL;
}
