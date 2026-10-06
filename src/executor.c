#include "executor.h"
#include "builtins.h"
#include "environment.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static JobList shell_jobs;
static int exit_requested;
static int last_command_status;
static int redirect_background_stdin(const Command *command);

static char *command_line_from_command(const Command *command)
{
    size_t length = 0;
    size_t i;
    char *line;
    char *cursor;

    for (i = 0; command->argv != NULL && command->argv[i] != NULL; i++)
        length += strlen(command->argv[i]) + (i == 0 ? 0 : 1);
    line = malloc(length + 1);
    if (line == NULL)
        return NULL;
    cursor = line;
    for (i = 0; command->argv != NULL && command->argv[i] != NULL; i++) {
        size_t argument_length = strlen(command->argv[i]);
        if (i != 0)
            *cursor++ = ' ';
        memcpy(cursor, command->argv[i], argument_length);
        cursor += argument_length;
    }
    *cursor = '\0';
    return line;
}

static char *command_line_from_pipeline(const Command *head)
{
    size_t length = 0;
    size_t stage_count = 0;
    const Command *command;
    char *line;
    char *cursor;

    for (command = head; command != NULL; command = command->next) {
        size_t i;
        for (i = 0; command->argv != NULL && command->argv[i] != NULL; i++)
            length += strlen(command->argv[i]) + (i == 0 ? 0 : 1);
        if (command->next != NULL)
            length += 3;
        stage_count++;
    }
    if (stage_count == 0)
        return NULL;
    line = malloc(length + 1);
    if (line == NULL)
        return NULL;
    cursor = line;
    for (command = head; command != NULL; command = command->next) {
        size_t i;
        for (i = 0; command->argv != NULL && command->argv[i] != NULL; i++) {
            size_t argument_length = strlen(command->argv[i]);
            if (i != 0)
                *cursor++ = ' ';
            memcpy(cursor, command->argv[i], argument_length);
            cursor += argument_length;
        }
        if (command->next != NULL) {
            memcpy(cursor, " | ", 3);
            cursor += 3;
        }
    }
    *cursor = '\0';
    return line;
}

JobList *shell_job_list(void)
{
    return &shell_jobs;
}

int shell_exit_requested(void)
{
    return exit_requested;
}

int shell_last_status(void)
{
    return last_command_status;
}

void shell_set_last_status(int status)
{
    last_command_status = status;
}

static int add_job(JobList *jobs, pid_t *pids, size_t pid_count,
                   char *command_line)
{
    Job *job = malloc(sizeof(*job));
    if (job == NULL)
        return -1;
    if (jobs->next_job_id <= 0)
        jobs->next_job_id = 1;
    job->job_id = jobs->next_job_id++;
    job->pids = pids;
    job->pid_count = pid_count;
    job->finished_count = 0;
    job->command_line = command_line;
    job->next = jobs->head;
    jobs->head = job;
    printf("[%d] %ld\n", job->job_id, (long)pids[pid_count - 1]);
    return 0;
}

static void exec_command(Command *cmd)
{
    char *path;
    int error_number;
    int should_exit;

    if (is_builtin(cmd->argv[0])) {
        int status = run_builtin(cmd, &should_exit);
        fflush(NULL);
        _exit(should_exit ? 0 : status);
    }

    path = resolve_executable_path(cmd->argv[0]);
    if (path == NULL) {
        fprintf(stderr, "%s: command not found\n", cmd->argv[0]);
        fflush(NULL);
        _exit(127);
    }
    execv(path, cmd->argv);
    error_number = errno;
    free(path);
    fprintf(stderr, "%s: %s\n", cmd->argv[0], strerror(error_number));
    fflush(NULL);
    _exit(error_number == ENOENT ? 127 : 126);
}

void run_in_background(Command *cmd, JobList *jobs)
{
    char *command_line;
    pid_t pid;

    if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL ||
        jobs == NULL) {
        fprintf(stderr, "cannot start an empty background command\n");
        return;
    }
    command_line = command_line_from_command(cmd);
    if (command_line == NULL) {
        fprintf(stderr, "background command: out of memory\n");
        return;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        free(command_line);
        return;
    }
    if (pid == 0) {
        if (redirect_background_stdin(cmd) < 0) {
            fflush(NULL);
            _exit(1);
        }
        if (apply_redirection(cmd) < 0) {
            fflush(NULL);
            _exit(1);
        }
        exec_command(cmd);
    }
    {
        pid_t *pids = malloc(sizeof(*pids));
        if (pids != NULL)
            pids[0] = pid;
        if (pids == NULL || add_job(jobs, pids, 1, command_line) < 0) {
            free(pids);
            free(command_line);
            fprintf(stderr, "background command: out of memory\n");
            if (waitpid(pid, NULL, 0) < 0)
                perror("waitpid");
        }
    }
}

void reap_finished_jobs(JobList *jobs)
{
    Job **link;
    int status;
    if (jobs == NULL)
        return;
    link = &jobs->head;
    while (*link != NULL) {
        Job *job = *link;
        size_t i;
        for (i = 0; i < job->pid_count; i++) {
            pid_t result;
            if (job->pids[i] <= 0)
                continue;
            result = waitpid(job->pids[i], &status, WNOHANG);
            if (result == job->pids[i] ||
                (result < 0 && errno == ECHILD)) {
                job->pids[i] = -1;
                job->finished_count++;
            } else if (result < 0 && errno != EINTR) {
                fprintf(stderr, "waitpid: %s\n", strerror(errno));
            }
        }
        if (job->finished_count == job->pid_count) {
            printf("[%d] Done %s\n", job->job_id,
                   job->command_line == NULL ? "(unknown command)" : job->command_line);
            *link = job->next;
            free(job->command_line);
            free(job->pids);
            free(job);
        } else {
            link = &job->next;
        }
    }
}

void free_job_list(JobList *jobs)
{
    Job *job;
    Job *next;
    if (jobs == NULL)
        return;
    job = jobs->head;
    while (job != NULL) {
        next = job->next;
        free(job->command_line);
        free(job->pids);
        free(job);
        job = next;
    }
    jobs->head = NULL;
}

void reap_background_jobs(void)
{
    reap_finished_jobs(&shell_jobs);
}

int apply_redirection(Command *cmd)
{
    int fd;
    if (cmd->input_file != NULL) {
        fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->input_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }
    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT |
                    (cmd->append_mode ? O_APPEND : O_TRUNC);
        fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->output_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

static size_t command_count(const Command *commands)
{
    size_t count = 0;
    while (commands != NULL) {
        count++;
        commands = commands->next;
    }
    return count;
}

static void wait_for_pids(const pid_t *pids, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (waitpid(pids[i], NULL, 0) < 0 && errno != ECHILD)
            fprintf(stderr, "waitpid: %s\n", strerror(errno));
    }
}

static void close_pipes(int (*pipes)[2], size_t pipe_count)
{
    size_t i;
    for (i = 0; i < pipe_count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

int execute_command(Command *cmd)
{
    pid_t pid;
    int status;

    if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL) {
        fprintf(stderr, "cannot execute an empty command\n");
        return 2;
    }
    if (cmd->next != NULL)
        return execute_pipeline(cmd);
    if (cmd->background) {
        run_in_background(cmd, &shell_jobs);
        return 0;
    }
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (apply_redirection(cmd) < 0)
            _exit(1);
        exec_command(cmd);
    }
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

static int redirect_background_stdin(const Command *command)
{
    int fd;
    if (command->input_file != NULL)
        return 0;
    fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
        perror("/dev/null");
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
        perror("dup2 /dev/null");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int execute_pipeline(Command *head)
{
    size_t command_total;
    size_t pipe_total;
    size_t i;
    Command *command;
    Command *last;
    int (*pipes)[2] = NULL;
    pid_t *pids = NULL;
    int last_status = 0;
    int background;

    if (head == NULL || head->argv == NULL || head->argv[0] == NULL)
        return 0;

    command_total = command_count(head);
    pipe_total = command_total - 1;
    last = head;
    while (last->next != NULL)
        last = last->next;
    background = last->background;

    pids = calloc(command_total, sizeof(*pids));
    if (pids == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    if (pipe_total > 0) {
        pipes = calloc(pipe_total, sizeof(*pipes));
        if (pipes == NULL) {
            fprintf(stderr, "out of memory\n");
            free(pids);
            return 1;
        }
        for (i = 0; i < pipe_total; i++) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                close_pipes(pipes, i);
                free(pipes);
                free(pids);
                return 1;
            }
        }
    }

    command = head;
    for (i = 0; i < command_total; i++, command = command->next) {
        fflush(stdout);
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            close_pipes(pipes, pipe_total);
            wait_for_pids(pids, i);
            free(pipes);
            free(pids);
            return 1;
        }
        if (pids[i] == 0) {
            int should_exit;

            if (background && i == 0 && redirect_background_stdin(command) < 0) {
                fflush(NULL);
                _exit(1);
            }
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                fflush(NULL);
                _exit(126);
            }
            if (i < pipe_total && dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                fflush(NULL);
                _exit(126);
            }
            close_pipes(pipes, pipe_total);
            if (apply_redirection(command) < 0) {
                fflush(NULL);
                _exit(1);
            }
            if (is_builtin(command->argv[0])) {
                int status = run_builtin(command, &should_exit);
                fflush(NULL);
                _exit(should_exit ? 0 : status);
            }
            exec_command(command);
        }
    }

    close_pipes(pipes, pipe_total);
    if (background) {
        char *command_line = command_line_from_pipeline(head);
        if (command_line == NULL) {
            fprintf(stderr, "background pipeline: out of memory\n");
            wait_for_pids(pids, command_total);
        } else {
            if (add_job(&shell_jobs, pids, command_total, command_line) < 0) {
                fprintf(stderr, "background pipeline: out of memory\n");
                wait_for_pids(pids, command_total);
                free(command_line);
            } else {
                pids = NULL;
            }
        }
    } else {
        for (i = 0; i < command_total; i++) {
            int status;
            if (waitpid(pids[i], &status, 0) < 0) {
                perror("waitpid");
                last_status = 1;
            } else if (i == command_total - 1) {
                if (WIFEXITED(status))
                    last_status = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                    last_status = 128 + WTERMSIG(status);
                else
                    last_status = 1;
            }
        }
    }
    free(pipes);
    free(pids);
    return last_status;
}

int execute_commands(Command *commands)
{
    size_t count;

    if (commands == NULL || commands->argv == NULL || commands->argv[0] == NULL)
        return 0;
    count = command_count(commands);
    if (count == 1 && !commands->background && is_builtin(commands->argv[0])) {
        int should_exit;
        int saved_in = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        int status;
        if (saved_in < 0 || saved_out < 0) {
            if (saved_in >= 0) close(saved_in);
            if (saved_out >= 0) close(saved_out);
            return 1;
        }
        if (apply_redirection(commands) < 0) {
            fflush(stdout);
            if (dup2(saved_in, STDIN_FILENO) < 0)
                perror("restore stdin");
            if (dup2(saved_out, STDOUT_FILENO) < 0)
                perror("restore stdout");
            close(saved_in);
            close(saved_out);
            return 1;
        }
        status = run_builtin(commands, &should_exit);
        fflush(stdout);
        if (dup2(saved_in, STDIN_FILENO) < 0 || dup2(saved_out, STDOUT_FILENO) < 0)
            fprintf(stderr, "failed to restore standard I/O: %s\n", strerror(errno));
        close(saved_in);
        close(saved_out);
        if (should_exit) {
            exit_requested = 1;
            return status;
        }
        return status;
    }
    return execute_command(commands);
}
