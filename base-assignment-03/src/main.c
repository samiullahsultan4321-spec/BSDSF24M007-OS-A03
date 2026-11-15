
#include "shell.h"

int main(void)
{
    char* cmdline;
    char** arglist;

    while (1) {

        // Read command
        cmdline = read_cmd(PROMPT, stdin);

        // Handle Ctrl+D / EOF
        if (cmdline == NULL) {
            printf("\nExiting MyShell...\n");
            break;
        }

        // Tokenize command
        arglist = tokenize(cmdline);

        if (arglist == NULL) {
            free(cmdline);
            continue;
        }

        // Ignore empty commands
        if (arglist[0] == NULL) {
            free(arglist);
            free(cmdline);
            continue;
        }

        // Check built-in commands FIRST
        if (handle_builtin(arglist) == 0) {

            // Only external commands reach execute()
            execute(arglist);
        }

        // Free allocated memory
        for (int i = 0; arglist[i] != NULL; i++) {
            free(arglist[i]);
        }

        free(arglist);
        free(cmdline);
    }

    return 0;
}
