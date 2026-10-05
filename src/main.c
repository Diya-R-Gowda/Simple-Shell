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
    int exit_status = 0;
    int reached_eof = 0;

    while (running) {
        Command *commands = NULL;
        char *error = NULL;
        ssize_t length;
        int status;
        reap_finished_jobs(shell_job_list());
        if (interactive) {
            fputs("myshell> ", stdout);
            fflush(stdout);
        }
        length = getline(&line, &capacity, stdin);
        if (length < 0) {
            reached_eof = 1;
            break;
        }
        if (length > 0 && line[length - 1] == '\n')
            line[length - 1] = '\0';
        if (parse_line(line, &commands, &error) < 0) {
            fprintf(stderr, "parse error: %s\n", error == NULL ? "unknown error" : error);
            free(error);
            shell_set_last_status(2);
            continue;
        }
        if (commands == NULL)
            continue;
        status = execute_commands(commands);
        shell_set_last_status(status);
        if (shell_exit_requested()) {
            running = 0;
            exit_status = status;
        }
        free_commands(commands);
    }
    if (interactive && reached_eof)
        putchar('\n');
    free(line);
    reap_background_jobs();
    free_job_list(shell_job_list());
    if (!running)
        return exit_status;
    exit_status = shell_last_status();
    return exit_status;
}
