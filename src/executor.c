#include "executor.h"
#include "builtins.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_JOBS 64
static pid_t jobs[MAX_JOBS];
static unsigned long job_numbers[MAX_JOBS];
static size_t job_count;
static unsigned long next_job = 1;

static void report_status(pid_t pid, int status)
{
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        fprintf(stderr, "process %ld exited with status %d\n", (long)pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        fprintf(stderr, "process %ld terminated by signal %d\n", (long)pid, WTERMSIG(status));
}

void reap_background_jobs(void)
{
    size_t i = 0;
    int status;
    while (i < job_count) {
        pid_t result = waitpid(jobs[i], &status, WNOHANG);
        if (result == 0) {
            i++;
        } else if (result == jobs[i]) {
            printf("[%lu] done (pid %ld)\n", jobs_numbers[i], (long)jobs[i]);
            report_status(jobs[i], status);
            jobs[i] = jobs[job_count - 1];
            jobs_numbers[i] = jobs_numbers[job_count - 1];
            job_count--;
        } else if (result < 0 && errno == ECHILD) {
            jobs[i] = jobs[job_count - 1];
            jobs_numbers[i] = jobs_numbers[job_count - 1];
            job_count--;
        } else {
            i++;
        }
    }
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

            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                _exit(126);
            if (i < pipe_total && dup2(pipes[i][1], STDOUT_FILENO) < 0)
                _exit(126);
            close_pipes(pipes, pipe_total);
            if (apply_redirection(command) < 0)
                _exit(1);
            if (is_builtin(command->argv[0])) {
                int status = run_builtin(command, &should_exit);
                _exit(should_exit ? 0 : status);
            }
            execvp(command->argv[0], command->argv);
            fprintf(stderr, "%s: %s\n", command->argv[0], strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }
    }

    close_pipes(pipes, pipe_total);
    if (background) {
        if (job_count + command_total > MAX_JOBS) {
            fprintf(stderr, "background job table is full; waiting for this job\n");
            wait_for_pids(pids, command_total);
        } else {
            unsigned long job_number = next_job++;
            for (i = 0; i < command_total; i++) {
                jobs[job_count] = pids[i];
                job_numbers[job_count++] = job_number;
            }
            printf("[%lu] %ld\n", job_number, (long)pids[command_total - 1]);
        }
    } else {
        for (i = 0; i < command_total; i++) {
            int status;
            if (waitpid(pids[i], &status, 0) < 0) {
                perror("waitpid");
                last_status = 1;
            } else if (i == command_total - 1) {
                last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
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
    reap_background_jobs();
    count = command_count(commands);
    if (count == 1 && !commands->background && is_builtin(commands->argv[0])) {
        int should_exit;
        int saved_in = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        int status;
        if (saved_in < 0 || saved_out < 0 || apply_redirection(commands) < 0) {
            if (saved_in >= 0) close(saved_in);
            if (saved_out >= 0) close(saved_out);
            return 1;
        }
        status = run_builtin(commands, &should_exit);
        if (dup2(saved_in, STDIN_FILENO) < 0 || dup2(saved_out, STDOUT_FILENO) < 0)
            fprintf(stderr, "failed to restore standard I/O: %s\n", strerror(errno));
        close(saved_in);
        close(saved_out);
        if (should_exit)
            return 1000 + status;
        return status;
    }
    return execute_pipeline(commands);
}
