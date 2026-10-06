#include "redirection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int has_parent_component(const char *path)
{
    const char *component = path;
    while (*component != '\0') {
        const char *end = strchr(component, '/');
        size_t length = end == NULL ? strlen(component) : (size_t)(end - component);
        if (length == 2 && component[0] == '.' && component[1] == '.')
            return 1;
        if (end == NULL)
            break;
        component = end + 1;
    }
    return 0;
}

static char *canonical_redirection_input(const char *path)
{
    if (path[0] == '\0') {
        fprintf(stderr, "redirection: empty path is not allowed\n");
        return NULL;
    }
    if (has_parent_component(path)) {
        fprintf(stderr, "redirection: '..' paths are not allowed: %s\n", path);
        return NULL;
    }
    {
        char *canonical_path = realpath(path, NULL);
        if (canonical_path == NULL)
            perror(path);
        return canonical_path;
    }
}

static char *canonical_redirection_output(const char *path)
{
    char *path_copy;
    char *separator;
    char *parent_path;
    char *canonical_parent;
    char *canonical_path;
    size_t length;

    if (path[0] == '\0') {
        fprintf(stderr, "redirection: empty path is not allowed\n");
        return NULL;
    }
    if (has_parent_component(path)) {
        fprintf(stderr, "redirection: '..' paths are not allowed: %s\n", path);
        return NULL;
    }
    path_copy = strdup(path);
    if (path_copy == NULL) {
        fprintf(stderr, "redirection: out of memory\n");
        return NULL;
    }
    separator = strrchr(path_copy, '/');
    if (separator == NULL) {
        parent_path = strdup(".");
    } else if (separator == path_copy) {
        parent_path = strdup("/");
    } else {
        *separator = '\0';
        parent_path = strdup(path_copy);
    }
    if (parent_path == NULL) {
        free(path_copy);
        fprintf(stderr, "redirection: out of memory\n");
        return NULL;
    }
    canonical_parent = realpath(parent_path, NULL);
    if (canonical_parent == NULL) {
        perror(path);
        free(parent_path);
        free(path_copy);
        return NULL;
    }
    length = strlen(canonical_parent) + 1 + strlen(separator == NULL
                                                        ? path_copy
                                                        : separator + 1) + 1;
    canonical_path = malloc(length);
    if (canonical_path == NULL) {
        fprintf(stderr, "redirection: out of memory\n");
        free(canonical_parent);
        free(parent_path);
        free(path_copy);
        return NULL;
    }
    snprintf(canonical_path, length, "%s/%s", canonical_parent,
             separator == NULL ? path_copy : separator + 1);
    free(canonical_parent);
    free(parent_path);
    free(path_copy);
    return canonical_path;
}

int apply_redirection(Command *cmd)
{
    int fd;
    if (cmd->input_file != NULL) {
        char *canonical_path = canonical_redirection_input(cmd->input_file);
        if (canonical_path == NULL)
            return -1;
        fd = open(canonical_path, O_RDONLY);
        free(canonical_path);
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
        char *canonical_path;
        int flags = O_WRONLY | O_CREAT |
                    (cmd->append_mode ? O_APPEND : O_TRUNC);
        canonical_path = canonical_redirection_output(cmd->output_file);
        if (canonical_path == NULL)
            return -1;
        fd = open(canonical_path, flags, 0644);
        free(canonical_path);
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
