#include "builtins.h"
#include "environment.h"
#include "executor.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int valid_variable_name(const char *name, size_t length)
{
    size_t i;
    if (length == 0 ||
        !(isalpha((unsigned char)name[0]) || name[0] == '_'))
        return 0;
    for (i = 1; i < length; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
            return 0;
    }
    return 1;
}

int is_builtin(const char *name)
{
    return name != NULL &&
           (strcmp(name, "cd") == 0 || strcmp(name, "pwd") == 0 ||
            strcmp(name, "echo") == 0 || strcmp(name, "help") == 0 ||
            strcmp(name, "exit") == 0 || strcmp(name, "export") == 0 ||
            strcmp(name, "unset") == 0 || strcmp(name, "jobs") == 0);
}

void print_help(void)
{
    puts("Built-ins: cd [dir], pwd, echo [args...], export NAME[=VALUE] [...],");
    puts("           unset NAME, jobs, help, exit [status]");
    puts("Operators: < input, > output, >> append, | pipeline, & background");
}

int run_builtin(const Command *command, int *should_exit)
{
    const char *name = command->argv[0];
    size_t argc = 0;
    int status = 0;
    *should_exit = 0;
    while (command->argv[argc] != NULL)
        argc++;
    if (strcmp(name, "cd") == 0) {
        const char *path = argc > 1 ? command->argv[1] : shell_getenv("HOME");
        if (argc > 2) {
            fprintf(stderr, "cd: too many arguments\n");
            return 2;
        }
        if (path == NULL || chdir(path) < 0) {
            fprintf(stderr, "cd: %s: %s\n", path == NULL ? "(HOME is unset)" : path,
                    path == NULL ? "HOME is not set" : strerror(errno));
            return 1;
        }
    } else if (strcmp(name, "pwd") == 0) {
        char cwd[4096];
        if (argc > 1 || getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "pwd: %s\n", argc > 1 ? "too many arguments" : strerror(errno));
            return 1;
        }
        puts(cwd);
    } else if (strcmp(name, "echo") == 0) {
        size_t i;
        for (i = 1; i < argc; i++)
            printf("%s%s", i == 1 ? "" : " ", command->argv[i]);
        putchar('\n');
    } else if (strcmp(name, "help") == 0) {
        if (argc > 1) {
            fprintf(stderr, "help: no arguments expected\n");
            return 2;
        }
        print_help();
    } else if (strcmp(name, "export") == 0) {
        size_t i;
        if (argc < 2) {
            fprintf(stderr, "export: usage: export NAME[=VALUE] [...]\n");
            return 2;
        }
        for (i = 1; i < argc; i++) {
            char *equals = strchr(command->argv[i], '=');
            size_t name_length = equals == NULL
                                     ? strlen(command->argv[i])
                                     : (size_t)(equals - command->argv[i]);
            if (!valid_variable_name(command->argv[i], name_length)) {
                fprintf(stderr, "export: invalid variable name: %s\n",
                        command->argv[i]);
                status = 2;
                continue;
            }
            if (equals == NULL) {
                if (shell_getenv(command->argv[i]) == NULL) {
                    fprintf(stderr, "export: %s is unset\n", command->argv[i]);
                    status = 1;
                }
            } else {
                *equals = '\0';
                if (setenv(command->argv[i], equals + 1, 1) < 0) {
                    fprintf(stderr, "export: %s\n", strerror(errno));
                    status = 1;
                }
                *equals = '=';
            }
        }
    } else if (strcmp(name, "unset") == 0) {
        size_t i;
        if (argc < 2) {
            fprintf(stderr, "unset: usage: unset NAME [...]\n");
            return 2;
        }
        for (i = 1; i < argc; i++) {
            if (unsetenv(command->argv[i]) < 0) {
                fprintf(stderr, "unset: %s: %s\n", command->argv[i], strerror(errno));
                status = 1;
            }
        }

    } else if (strcmp(name, "jobs") == 0) {
        JobList *jobs = shell_job_list();
        Job *job;

        for (job = jobs->head; job != NULL; job = job->next) {
            printf("[%d] Running %s\n",
                job->job_id,
                job->command_line == NULL ? "(unknown command)" : job->command_line);
        }

    } else if (strcmp(name, "exit") == 0) {
        char *end;
        long value;
        if (argc > 2) {
            fprintf(stderr, "exit: too many arguments\n");
            return 2;
        }
        if (argc == 2) {
            errno = 0;
            value = strtol(command->argv[1], &end, 10);
            if (errno != 0 || *end != '\0' || value < 0 || value > 255) {
                fprintf(stderr, "exit: expected a status from 0 to 255\n");
                return 2;
            }
            status = (int)value;
        }
        *should_exit = 1;
    }
    return status;
}
