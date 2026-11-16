#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#define MAXARGS 100
#define HISTORY_SIZE 20
#define PROMPT "myshell> "

// Custom command history
typedef struct {
    char* commands[HISTORY_SIZE];
    unsigned long total;
} History;

// Parsed command
typedef struct Command {
    char** argv;
    int argc;

    char* input_file;
    char* output_file;

    struct Command* next;
} Command;

// Parser functions
Command* tokenize(char* cmdline);
void free_commands(Command* commands);

// Execution
int execute_pipeline(Command* commands);

// Built-in and history functions
int handle_builtin(char* arglist[], History* history);
void history_add(History* history, const char* command);
const char* custom_history_get(History* history, unsigned long number);
void display_history(History* history);

#endif