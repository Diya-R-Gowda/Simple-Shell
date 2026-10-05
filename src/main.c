#include "executor.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char *line = NULL;
    size_t capacity = 0;
    int interactive = isatty(STDIN_FILENO);
    int running = 1;

    while (running) {
        Command *commands = NULL;
        char *error = NULL;
        ssize_t length;
        int status;
        if (interactive) {
            fputs("myshell> ", stdout);
            fflush(stdout);
        }
        length = getline(&line, &capacity, stdin);
        if (length < 0)
            break;
        if (length > 0 && line[length - 1] == '\n')
            line[length - 1] = '\0';
        if (parse_line(line, &commands, &error) < 0) {
            fprintf(stderr, "parse error: %s\n", error == NULL ? "unknown error" : error);
            free(error);
            continue;
        }
        if (commands == NULL)
            continue;
        status = execute_commands(commands);
        if (status >= 1000)
            running = 0;
        free_commands(commands);
    }
    free(line);
    reap_background_jobs();
    return 0;
}
