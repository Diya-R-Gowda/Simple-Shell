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

static int apply_redirection(const Command *command)
{
    int fd;
    if (command->input_file != NULL) {
        fd = open(command->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s: %s\n", command->input_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "dup2: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }
    if (command->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (command->append_mode ? O_APPEND : O_TRUNC);
        fd = open(command->output_file, flags, 0666);
        if (fd < 0) {
            fprintf(stderr, "%s: %s\n", command->output_file, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "dup2: %s\n", strerror(errno));
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

int execute_commands(Command *commands)
{
    size_t count;
    pid_t *pids;
    size_t i;
    int previous_read = -1;
    int last_status = 0;
    int background;

    if (commands == NULL || commands->argv == NULL || commands->argv[0] == NULL)
        return 0;
    reap_background_jobs();
    count = command_count(commands);
    background = commands->background;
    if (count == 1 && !background && is_builtin(commands->argv[0])) {
        int should_exit;
        int saved_in = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        if (saved_in < 0 || saved_out < 0 || apply_redirection(commands) < 0) {
            if (saved_in >= 0) close(saved_in);
            if (saved_out >= 0) close(saved_out);
            return 1;
        }
        last_status = run_builtin(commands, &should_exit);
        if (dup2(saved_in, STDIN_FILENO) < 0 || dup2(saved_out, STDOUT_FILENO) < 0)
            fprintf(stderr, "failed to restore standard I/O: %s\n", strerror(errno));
        close(saved_in);
        close(saved_out);
        if (should_exit)
            return 1000 + last_status;
        return last_status;
    }
    pids = calloc(count, sizeof(*pids));
    if (pids == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    for (i = 0; i < count; i++) {
        int pipe_fds[2] = {-1, -1};
        Command *command = commands;
        size_t j;
        for (j = 0; j < i; j++)
            command = command->next;
        if (command->next != NULL && pipe(pipe_fds) < 0) {
            fprintf(stderr, "pipe: %s\n", strerror(errno));
            if (previous_read >= 0) close(previous_read);
            wait_for_pids(pids, i);
            free(pids);
            return 1;
        }
        pids[i] = fork();
        if (pids[i] < 0) {
            fprintf(stderr, "fork: %s\n", strerror(errno));
            if (pipe_fds[0] >= 0) close(pipe_fds[0]);
            if (pipe_fds[1] >= 0) close(pipe_fds[1]);
            if (previous_read >= 0) close(previous_read);
            wait_for_pids(pids, i);
            free(pids);
            return 1;
        }
        if (pids[i] == 0) {
            int should_exit;
            if (previous_read >= 0 && dup2(previous_read, STDIN_FILENO) < 0)
                _exit(126);
            if (pipe_fds[1] >= 0 && dup2(pipe_fds[1], STDOUT_FILENO) < 0)
                _exit(126);
            if (previous_read >= 0) close(previous_read);
            if (pipe_fds[0] >= 0) close(pipe_fds[0]);
            if (pipe_fds[1] >= 0) close(pipe_fds[1]);
            if (apply_redirection(command) < 0)
                _exit(1);
            if (is_builtin(command->argv[0])) {
                int result = run_builtin(command, &should_exit);
                _exit(should_exit ? 0 : result);
            }
            execvp(command->argv[0], command->argv);
            fprintf(stderr, "%s: %s\n", command->argv[0], strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }
        if (previous_read >= 0)
            close(previous_read);
        if (pipe_fds[1] >= 0)
            close(pipe_fds[1]);
        previous_read = pipe_fds[0];
    }
    if (previous_read >= 0)
        close(previous_read);
    if (background) {
        if (job_count + count > MAX_JOBS) {
            fprintf(stderr, "background job table is full; waiting for this job\n");
            wait_for_pids(pids, count);
        } else {
            unsigned long job_number = next_job++;
            for (i = 0; i < count; i++) {
                jobs[job_count] = pids[i];
                jobs_numbers[job_count++] = job_number;
            }
            printf("[%lu] %ld\n", job_number, (long)pids[count - 1]);
        }
    } else {
        for (i = 0; i < count; i++) {
            int status;
            if (waitpid(pids[i], &status, 0) < 0) {
                fprintf(stderr, "waitpid: %s\n", strerror(errno));
                last_status = 1;
            } else {
                last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            }
        }
    }
    free(pids);
    return last_status;
}
