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
#define MAX_JOBS 100
#define PROMPT "myshell> "

// Custom command history
typedef struct {
    char* commands[HISTORY_SIZE];
    unsigned long total;
} History;

// Variable node for storage
typedef struct VarNode {
    char* name;
    char* value;
    struct VarNode* next;
} VarNode;

// Background Job representation
typedef struct {
    pid_t pid;
    int job_id;
    char* command_str;
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int count;
    int next_id;
} JobList;

// Parsed command
typedef struct Command {
    char** argv;
    int argc;

    char* input_file;
    char* output_file;
    int background; // 1 if execution is in background (&)

    struct Command* next;
} Command;

// Parser functions
Command* tokenize(char* cmdline);
void free_commands(Command* commands);
char** split_command_lines(char* cmdline, int* num_cmds);
int is_assignment(const char* str, char** name_out, char** val_out);

// Variable functions
void var_set(VarNode** head, const char* name, const char* value);
const char* var_get(VarNode* head, const char* name);
void var_free(VarNode* head);
void expand_command_variables(Command* commands, VarNode* vars);

// Execution
int execute_pipeline(Command* commands, JobList* job_list, const char* original_cmd);

// Built-in and history functions
int handle_builtin(char* arglist[], History* history, JobList* job_list, VarNode* vars);
void history_add(History* history, const char* command);
const char* custom_history_get(History* history, unsigned long number);
void display_history(History* history);

// Job management functions
void job_list_init(JobList* list);
void job_list_add(JobList* list, pid_t pid, const char* cmd_str);
void job_list_remove(JobList* list, pid_t pid);
void job_list_print(JobList* list);

#endif