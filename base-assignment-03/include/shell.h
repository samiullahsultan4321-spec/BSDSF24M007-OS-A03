
#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LEN 1024
#define MAXARGS 100
#define HISTORY_SIZE 20
#define PROMPT "myshell> "

// History structure
typedef struct {
    char* commands[HISTORY_SIZE];
    unsigned long total;
} History;

// Existing functions
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char* arglist[]);

// Built-in and history functions
int handle_builtin(char* arglist[], History* history);
void history_add(History* history, const char* command);
const char* history_get(History* history, unsigned long number);

#endif
