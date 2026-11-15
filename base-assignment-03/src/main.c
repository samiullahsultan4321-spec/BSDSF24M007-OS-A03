
#include "shell.h"

int main(void)
{
    char* cmdline;
    char** arglist;

    // Initialize history
    History history = {0};

    while (1) {

        // Read command
        cmdline = read_cmd(PROMPT, stdin);

        // Handle Ctrl+D
        if (cmdline == NULL) {
            printf("\nExiting MyShell...\n");
            break;
        }

        // Ignore empty commands
        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        // Check for !n command BEFORE adding to history
        if (cmdline[0] == '!') {

            char* endptr;

            // The character after ! must be a digit
            if (!isdigit((unsigned char)cmdline[1])) {
                printf("Invalid history command.\n");
                free(cmdline);
                continue;
            }

            errno = 0;
            unsigned long number = strtoul(cmdline + 1, &endptr, 10);

            // Allow trailing spaces, but reject other characters
            while (isspace((unsigned char)*endptr)) {
                endptr++;
            }

            if (errno == ERANGE || *endptr != '\0') {
                printf("Invalid history number.\n");
                free(cmdline);
                continue;
            }

            // Retrieve requested command
            const char* previous = history_get(&history, number);

            if (previous == NULL) {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }

            // Replace !n with the actual command
            strcpy(cmdline, previous);

            printf("%s\n", cmdline);
        }

        // Store actual command in history
        history_add(&history, cmdline);

        // Tokenize command
        arglist = tokenize(cmdline);

        if (arglist == NULL) {
            free(cmdline);
            continue;
        }

        // Ignore empty token list
        if (arglist[0] == NULL) {
            free(arglist);
            free(cmdline);
            continue;
        }

        // Check built-in commands first
        if (handle_builtin(arglist, &history) == 0) {

            // Execute external commands
            execute(arglist);
        }

        // Free allocated argument memory
        for (int i = 0; arglist[i] != NULL; i++) {
            free(arglist[i]);
        }

        free(arglist);
        free(cmdline);
    }

    // Free history memory
    for (int i = 0; i < HISTORY_SIZE; i++) {
        free(history.commands[i]);
    }

    return 0;
}
