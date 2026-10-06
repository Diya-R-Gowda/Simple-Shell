#include "executor.h"
#include "limits.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char line[MAX_LINE_LENGTH + 1];
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
        if (fgets(line, sizeof(line), stdin) == NULL) {
            reached_eof = 1;
            break;
        }
        length = (ssize_t)strlen(line);
        if (length == MAX_LINE_LENGTH && line[length - 1] != '\n') {
            int character;
            int too_long = 0;
            while ((character = fgetc(stdin)) != '\n' && character != EOF)
                too_long = 1;
            if (too_long || (character != '\n' && character != EOF)) {
                fprintf(stderr, "input line too long (max 4096 characters)\n");
                shell_set_last_status(2);
                continue;
            }
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
    reap_background_jobs();
    free_job_list(shell_job_list());
    if (!running)
        return exit_status;
    exit_status = shell_last_status();
    return exit_status;
}
