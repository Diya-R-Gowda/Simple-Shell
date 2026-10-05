#include "parser.h"
#include "environment.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *text;
    int is_operator;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} StringList;

static void list_free(StringList *list)
{
    size_t i;
    for (i = 0; i < list->count; i++)
        free(list->items[i].text);
    free(list->items);
}

static int list_add(StringList *list, char *item, int is_operator)
{
    Token *grown;
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        grown = realloc(list->items, capacity * sizeof(*grown));
        if (grown == NULL)
            return -1;
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count].text = item;
    list->items[list->count].is_operator = is_operator;
    list->count++;
    return 0;
}

static int append_char(char **value, size_t *length, size_t *capacity, char c)
{
    char *grown;
    if (*length + 1 >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        grown = realloc(*value, new_capacity);
        if (grown == NULL)
            return -1;
        *value = grown;
        *capacity = new_capacity;
    }
    (*value)[(*length)++] = c;
    (*value)[*length] = '\0';
    return 0;
}

static int variable_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static char *expand_variable(const char *line, size_t *position, char *error)
{
    size_t start = *position;
    size_t length = 0;
    const char *name;
    char *result;
    const char *value;

    if (line[*position] == '{') {
        start = ++(*position);
        while (line[*position] != '\0' && line[*position] != '}')
            (*position)++;
        if (line[*position] != '}') {
            snprintf(error, 256, "missing '}' in variable expansion");
            return NULL;
        }
        length = *position - start;
        {
            size_t i;
            for (i = 0; i < length; i++) {
                if (!variable_name_char(line[start + i])) {
                    snprintf(error, 256, "invalid variable name");
                    return NULL;
                }
            }
        }
        (*position)++;
    } else {
        while (variable_name_char(line[*position]))
            (*position)++;
        length = *position - start;
    }
    if (length == 0) {
        snprintf(error, 256, "invalid variable expansion");
        return NULL;
    }
    name = line + start;
    result = malloc(length + 1);
    if (result == NULL)
        return NULL;
    memcpy(result, name, length);
    result[length] = '\0';
    value = shell_getenv(result);
    free(result);
    return strdup(value == NULL ? "" : value);
}

static char *parse_word(const char *line, size_t *position, char *error)
{
    char *word = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int quoted = 0;
    char quote = '\0';

    while (line[*position] != '\0') {
        char c = line[*position];
        if (!quoted && (isspace((unsigned char)c) || c == '|' || c == '<' ||
                        c == '>' || c == '&'))
            break;
        if (c == '\\' && (!quoted || quote == '"')) {
            (*position)++;
            if (line[*position] == '\0') {
                snprintf(error, 256, "trailing escape");
                free(word);
                return NULL;
            }
            if (append_char(&word, &length, &capacity, line[(*position)++]) < 0)
                goto allocation_error;
            continue;
        }
        if ((c == '\'' || c == '"')) {
            if (!quoted) {
                quoted = 1;
                quote = c;
            } else if (quote == c) {
                quoted = 0;
            } else if (append_char(&word, &length, &capacity, c) < 0) {
                goto allocation_error;
            }
            (*position)++;
            continue;
        }
        if (c == '$' && (!quoted || quote == '"')) {
            char *value;
            (*position)++;
            if (line[*position] == '\0') {
                if (append_char(&word, &length, &capacity, '$') < 0)
                    goto allocation_error;
                break;
            }
            if (line[*position] == '$') {
                char pid_text[32];
                snprintf(pid_text, sizeof(pid_text), "%ld", (long)getpid());
                value = strdup(pid_text);
                (*position)++;
            } else if (line[*position] != '{' &&
                       !variable_name_char(line[*position])) {
                if (append_char(&word, &length, &capacity, '$') < 0)
                    goto allocation_error;
                continue;
            } else {
                value = expand_variable(line, position, error);
            }
            if (value == NULL)
                goto parse_error;
            {
                char *value_cursor = value;
                while (*value_cursor != '\0') {
                    if (append_char(&word, &length, &capacity, *value_cursor++) < 0) {
                        free(value);
                        goto allocation_error;
                    }
                }
                free(value);
            }
            continue;
        }
        if (append_char(&word, &length, &capacity, c) < 0)
            goto allocation_error;
        (*position)++;
    }
    if (quoted) {
        snprintf(error, 256, "unmatched %c quote", quote);
        free(word);
        return NULL;
    }
    if (word == NULL)
        word = strdup("");
    return word;

allocation_error:
    snprintf(error, 256, "out of memory");
parse_error:
    free(word);
    return NULL;
}

static Command *new_command(void)
{
    return calloc(1, sizeof(Command));
}

int parse_line(const char *line, Command **commands, char **error_message)
{
    Command *head = NULL;
    Command *current = NULL;
    StringList words = {0};
    size_t position = 0;
    char error[256] = "out of memory";
    int saw_token = 0;

    *commands = NULL;
    *error_message = NULL;
    while (line[position] != '\0') {
        char c;
        while (isspace((unsigned char)line[position]))
            position++;
        if (line[position] == '\0')
            break;
        c = line[position];
        if (c == '|' || c == '<' || c == '>' || c == '&') {
            char *operator = NULL;
            if (c == '>' && line[position + 1] == '>') {
                operator = strdup(">>");
                position += 2;
            } else {
                operator = malloc(2);
                if (operator != NULL) {
                    operator[0] = c;
                    operator[1] = '\0';
                }
                position++;
            }
            if (operator == NULL || list_add(&words, operator, 1) < 0) {
                free(operator);
                snprintf(error, sizeof(error), "out of memory");
                goto fail;
            }
            saw_token = 1;
        } else {
            char *word = parse_word(line, &position, error);
            if (word == NULL)
                goto fail;
            if (list_add(&words, word, 0) < 0) {
                free(word);
                snprintf(error, sizeof(error), "out of memory");
                goto fail;
            }
            saw_token = 1;
        }
    }
    if (!saw_token) {
        list_free(&words);
        return 0;
    }

    current = new_command();
    if (current == NULL)
        goto fail;
    head = current;
    for (position = 0; position < words.count; position++) {
        char *token = words.items[position].text;
        int is_operator = words.items[position].is_operator;
        if (is_operator && strcmp(token, "|") == 0) {
            if (current->argv == NULL || current->argv[0] == NULL) {
                snprintf(error, sizeof(error), "pipe requires commands on both sides");
                goto fail;
            }
            current->next = new_command();
            if (current->next == NULL)
                goto fail;
            current = current->next;
        } else if (is_operator &&
                   (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 ||
                    strcmp(token, ">>") == 0)) {
            char **target;
            if (position + 1 >= words.count ||
                words.items[position + 1].text[0] == '\0' ||
                words.items[position + 1].is_operator) {
                snprintf(error, sizeof(error), "redirection requires a file name");
                goto fail;
            }
            target = strcmp(token, "<") == 0 ? &current->input_file : &current->output_file;
            if (*target != NULL) {
                snprintf(error, sizeof(error), "duplicate redirection");
                goto fail;
            }
            *target = strdup(words.items[++position].text);
            if (*target == NULL)
                goto fail;
            if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0)
                current->append_mode = strcmp(token, ">>") == 0;
        } else if (is_operator && strcmp(token, "&") == 0) {
            if (position + 1 != words.count) {
                snprintf(error, sizeof(error), "'&' must be at the end of a command");
                goto fail;
            }
            if (current->background) {
                snprintf(error, sizeof(error), "duplicate background operator");
                goto fail;
            }
            current->background = 1;
        } else {
            size_t count = 0;
            char **grown;
            while (current->argv != NULL && current->argv[count] != NULL)
                count++;
            grown = realloc(current->argv, (count + 2) * sizeof(*grown));
            if (grown == NULL)
                goto fail;
            current->argv = grown;
            current->argv[count] = strdup(token);
            if (current->argv[count] == NULL)
                goto fail;
            current->argv[count + 1] = NULL;
        }
    }
    if (current->argv == NULL || current->argv[0] == NULL) {
        snprintf(error, sizeof(error), "command is missing");
        goto fail;
    }
    if (current->background) {
        for (current = head; current != NULL; current = current->next)
            current->background = 1;
    }
    list_free(&words);
    *commands = head;
    return 0;

fail:
    list_free(&words);
    free_commands(head);
    *error_message = strdup(error);
    return -1;
}

void free_commands(Command *commands)
{
    while (commands != NULL) {
        Command *next = commands->next;
        size_t i = 0;
        if (commands->argv != NULL) {
            while (commands->argv[i] != NULL)
                free(commands->argv[i++]);
            free(commands->argv);
        }
        free(commands->input_file);
        free(commands->output_file);
        free(commands);
        commands = next;
    }
}
