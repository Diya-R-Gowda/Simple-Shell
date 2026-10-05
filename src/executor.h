#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"
#include <sys/types.h>

typedef struct Job {
    int job_id;
    pid_t pid;
    char *command_line;
    struct Job *next;
} Job;

typedef struct {
    Job *head;
    int next_job_id;
} JobList;

int apply_redirection(Command *cmd);
void run_in_background(Command *cmd, JobList *jobs);
void reap_finished_jobs(JobList *jobs);
JobList *shell_job_list(void);
int execute_pipeline(Command *head);
int execute_commands(Command *commands);
void reap_background_jobs(void);

#endif
