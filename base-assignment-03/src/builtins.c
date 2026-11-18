#include "shell.h"

// Initialize job list
void job_list_init(JobList* list)
{
    list->count = 0;
    list->next_id = 1;
    for (int i = 0; i < MAX_JOBS; i++) {
        list->jobs[i].pid = 0;
        list->jobs[i].job_id = 0;
        list->jobs[i].command_str = NULL;
    }
}

// Add a job to the background list
void job_list_add(JobList* list, pid_t pid, const char* cmd_str)
{
    if (list->count >= MAX_JOBS) {
        return;
    }

    int idx = list->count;
    list->jobs[idx].pid = pid;
    list->jobs[idx].job_id = list->next_id++;
    list->jobs[idx].command_str = strdup(cmd_str);
    list->count++;
}

// Remove a job by PID
void job_list_remove(JobList* list, pid_t pid)
{
    for (int i = 0; i < list->count; i++) {
        if (list->jobs[i].pid == pid) {
            free(list->jobs[i].command_str);
            for (int j = i; j < list->count - 1; j++) {
                list->jobs[j] = list->jobs[j + 1];
            }
            list->count--;
            break;
        }
    }
}

// Print active background jobs
void job_list_print(JobList* list)
{
    for (int i = 0; i < list->count; i++) {
        printf("[%d] Running   %s\n", list->jobs[i].job_id, list->jobs[i].command_str);
    }
}

// Add a command to history
void history_add(History* history, const char* command)
{
    unsigned long index = history->total % HISTORY_SIZE;
    char* copy = malloc(strlen(command) + 1);
    if (copy == NULL) {
        perror("malloc");
        return;
    }
    strcpy(copy, command);
    free(history->commands[index]);
    history->commands[index] = copy;
    history->total++;
}

// Retrieve a command using its history number
const char* custom_history_get(History* history, unsigned long number)
{
    if (number == 0 || number > history->total) {
        return NULL;
    }

    unsigned long oldest = 1;
    if (history->total > HISTORY_SIZE) {
        oldest = history->total - HISTORY_SIZE + 1;
    }

    if (number < oldest) {
        return NULL;
    }

    unsigned long index = (number - 1) % HISTORY_SIZE;
    return history->commands[index];
}

// Display command history
void display_history(History* history)
{
    unsigned long oldest = 1;
    if (history->total > HISTORY_SIZE) {
        oldest = history->total - HISTORY_SIZE + 1;
    }

    for (unsigned long i = oldest; i <= history->total; i++) {
        const char* command = custom_history_get(history, i);
        if (command != NULL) {
            printf("%lu  %s\n", i, command);
        }
    }
}

// Handle built-in commands
int handle_builtin(char* arglist[], History* history, JobList* job_list, VarNode* vars)
{
    if (arglist[0] == NULL) {
        return 1;
    }

    // EXIT
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting MyShell...\n");
        exit(EXIT_SUCCESS);
    }

    // CD
    else if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            char* home = getenv("HOME");
            if (home == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(home) != 0) {
                perror("cd");
            }
        } else if (arglist[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
        } else {
            if (chdir(arglist[1]) != 0) {
                perror("cd");
            }
        }
        return 1;
    }

    // HELP
    else if (strcmp(arglist[0], "help") == 0) {
        printf("\n===== MyShell Built-in Commands =====\n");
        printf("exit             Exit the shell\n");
        printf("cd <directory>   Change directory\n");
        printf("help             Display help\n");
        printf("jobs             Display job status\n");
        printf("history          Display command history\n");
        printf("set              Display shell variables\n");
        printf("!n               Execute command number n\n");
        printf("=====================================\n\n");
        return 1;
    }

    // JOBS
    else if (strcmp(arglist[0], "jobs") == 0) {
        job_list_print(job_list);
        return 1;
    }

    // HISTORY
    else if (strcmp(arglist[0], "history") == 0) {
        display_history(history);
        return 1;
    }

    // SET (Display variables)
    else if (strcmp(arglist[0], "set") == 0) {
        VarNode* current = vars;
        while (current != NULL) {
            printf("%s=%s\n", current->name, current->value);
            current = current->next;
        }
        return 1;
    }

    // Not a built-in command
    return 0;
}