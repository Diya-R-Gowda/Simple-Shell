#ifndef PARSER_H
#define PARSER_H

typedef struct Command {
    char **argv;
    char *input_file;
    char *output_file;
    int append_mode;
    struct Command *next;
    int background;
} Command;

int parse_line(const char *line, Command **commands, char **error_message);
void free_commands(Command *commands);

#endif
