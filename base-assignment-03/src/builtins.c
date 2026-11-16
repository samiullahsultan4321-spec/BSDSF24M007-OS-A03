
#include "shell.h"

// Add a command to history
void history_add(History* history, const char* command)
{
    unsigned long index = history->total % HISTORY_SIZE;

    // Allocate memory for the new command
    char* copy = malloc(strlen(command) + 1);

    if (copy == NULL) {
        perror("malloc");
        return;
    }

    strcpy(copy, command);

    // Free old command before overwriting its slot
    free(history->commands[index]);

    history->commands[index] = copy;

    history->total++;
}

// Retrieve a command using its history number
const char* custom_history_get(History* history, unsigned long number)
{
    // Check whether number is valid
    if (number == 0 || number > history->total) {
        return NULL;
    }

    // Find oldest available command number
    unsigned long oldest = 1;

    if (history->total > HISTORY_SIZE) {
        oldest = history->total - HISTORY_SIZE + 1;
    }

    if (number < oldest) {
        return NULL;
    }

    // Calculate circular array index
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
int handle_builtin(char* arglist[], History* history)
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
            }
            else if (chdir(home) != 0) {
                perror("cd");
            }
        }

        else if (arglist[2] != NULL) {

            fprintf(stderr, "cd: too many arguments\n");
        }

        else {

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
        printf("!n               Execute command number n\n");
        printf("=====================================\n\n");

        return 1;
    }

    // JOBS
    else if (strcmp(arglist[0], "jobs") == 0) {

        printf("Job control not yet implemented.\n");

        return 1;
    }

    // HISTORY
    else if (strcmp(arglist[0], "history") == 0) {

        display_history(history);

        return 1;
    }

    // Not a built-in command
    return 0;
}
