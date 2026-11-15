
#include "shell.h"

int handle_builtin(char* arglist[])
{
    // Check for empty command
    if (arglist[0] == NULL) {
        return 1;
    }

    // 1. EXIT COMMAND
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting MyShell...\n");
        exit(EXIT_SUCCESS);
    }

    // 2. CD COMMAND
    else if (strcmp(arglist[0], "cd") == 0) {

        // If no directory is provided, go to HOME
        if (arglist[1] == NULL) {

            char* home = getenv("HOME");

            if (home == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }

            if (chdir(home) != 0) {
                perror("cd");
            }
        }

        // More than one directory argument
        else if (arglist[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
        }

        // Change directory
        else {
            if (chdir(arglist[1]) != 0) {
                perror("cd");
            }
        }

        return 1;
    }

    // 3. HELP COMMAND
    else if (strcmp(arglist[0], "help") == 0) {

        printf("\n===== MyShell Built-in Commands =====\n");
        printf("exit          Exit the shell\n");
        printf("cd <directory> Change current directory\n");
        printf("help          Display available commands\n");
        printf("jobs          Display job control status\n");
        printf("=====================================\n\n");

        return 1;
    }

    // 4. JOBS COMMAND
    else if (strcmp(arglist[0], "jobs") == 0) {

        printf("Job control not yet implemented.\n");

        return 1;
    }

    // Not a built-in command
    return 0;
}
