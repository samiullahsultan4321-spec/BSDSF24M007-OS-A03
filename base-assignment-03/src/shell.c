
#include "shell.h"

char* read_cmd(char* prompt, FILE* fp)
{
    char* cmdline = malloc(MAX_LEN * sizeof(char));

    if (cmdline == NULL) {
        perror("malloc");
        return NULL;
    }

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(cmdline, MAX_LEN, fp) == NULL) {
        free(cmdline);
        return NULL;
    }

    cmdline[strcspn(cmdline, "\n")] = '\0';

    return cmdline;
}

char** tokenize(char* cmdline)
{
    char** arglist = malloc((MAXARGS + 1) * sizeof(char*));

    if (arglist == NULL) {
        perror("malloc");
        return NULL;
    }

    int i = 0;
    char* token = strtok(cmdline, " \t");

    while (token != NULL && i < MAXARGS) {
        arglist[i] = malloc(strlen(token) + 1);

        if (arglist[i] == NULL) {
            perror("malloc");

            for (int j = 0; j < i; j++) {
                free(arglist[j]);
            }

            free(arglist);
            return NULL;
        }

        strcpy(arglist[i], token);

        i++;
        token = strtok(NULL, " \t");
    }

    arglist[i] = NULL;

    return arglist;
}
