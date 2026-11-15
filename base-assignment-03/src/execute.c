
#include "shell.h"

int execute(char* arglist[])
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    // Child process
    if (pid == 0) {

        execvp(arglist[0], arglist);

        // Only executes if execvp fails
        perror(arglist[0]);
        exit(EXIT_FAILURE);
    }

    // Parent process
    else {

        int status;

        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
    }

    return 0;
}
