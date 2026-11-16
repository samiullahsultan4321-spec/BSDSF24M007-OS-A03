
#include "shell.h"

#include <dirent.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>

// Variables used by command completion
static char* path_copy = NULL;
static char* path_saveptr = NULL;
static char* current_path = NULL;
static DIR* current_dir = NULL;


// Clean up command completion resources
static void cleanup_generator(void)
{
    if (current_dir != NULL) {
        closedir(current_dir);
        current_dir = NULL;
    }

    free(path_copy);
    path_copy = NULL;

    path_saveptr = NULL;
    current_path = NULL;
}


// Generate executable command names from PATH
static char* command_generator(const char* text, int state)
{
    struct dirent* entry;
    struct stat file_info;

    // Initialize when a new completion request starts
    if (state == 0) {
        cleanup_generator();

        const char* path = getenv("PATH");

        if (path == NULL) {
            return NULL;
        }

        path_copy = strdup(path);

        if (path_copy == NULL) {
            return NULL;
        }
    }

    if (path_copy == NULL) {
        return NULL;
    }

    while (1) {

        // Open the next PATH directory
        if (current_dir == NULL) {

            current_path = strtok_r(path_copy, ":", &path_saveptr);

            if (current_path == NULL) {
                cleanup_generator();
                return NULL;
            }

            current_dir = opendir(
                *current_path == '\0' ? "." : current_path
            );

            if (current_dir == NULL) {
                continue;
            }
        }

        // Search for matching executable files
        while ((entry = readdir(current_dir)) != NULL) {

            if (strncmp(entry->d_name, text, strlen(text)) != 0) {
                continue;
            }

            size_t dir_len = strlen(current_path);
            size_t name_len = strlen(entry->d_name);

            char* full_path = malloc(dir_len + name_len + 2);

            if (full_path == NULL) {
                continue;
            }

            snprintf(
                full_path,
                dir_len + name_len + 2,
                "%s/%s",
                current_path,
                entry->d_name
            );

            int valid =
                stat(full_path, &file_info) == 0 &&
                S_ISREG(file_info.st_mode) &&
                access(full_path, X_OK) == 0;

            free(full_path);

            if (valid) {
                return strdup(entry->d_name);
            }
        }

        // Move to the next directory
        closedir(current_dir);
        current_dir = NULL;
    }
}


// Handle both command and filename completion
static char** shell_completion(const char* text, int start, int end)
{
    (void)end;

    // At the beginning of the line, complete commands
    if (start == 0 && strchr(text, '/') == NULL) {
        rl_attempted_completion_over = 0;
        return rl_completion_matches(text, command_generator);
    }

    // Otherwise, use Readline's default filename completion
    return NULL;
}


// Check whether a command contains only whitespace
static int is_blank(const char* command)
{
    while (*command != '\0') {
        if (!isspace((unsigned char)*command)) {
            return 0;
        }

        command++;
    }

    return 1;
}


int main(void)
{
    char* cmdline;
    char** arglist;

    // Initialize custom history
    History history = {0};

    // Initialize GNU Readline history
    using_history();
    stifle_history(HISTORY_SIZE);

    // Enable command and filename completion
    rl_attempted_completion_function = shell_completion;

    while (1) {

        // Read command using GNU Readline
        cmdline = readline(PROMPT);

        // Handle Ctrl+D
        if (cmdline == NULL) {
            printf("\nExiting MyShell...\n");
            break;
        }

        // Ignore empty or whitespace-only commands
        if (is_blank(cmdline)) {
            free(cmdline);
            continue;
        }

        // Check for !n before adding the command to history
        if (cmdline[0] == '!') {

            char* endptr;

            if (!isdigit((unsigned char)cmdline[1])) {
                printf("Invalid history command.\n");
                free(cmdline);
                continue;
            }

            errno = 0;

            unsigned long number = strtoul(
                cmdline + 1,
                &endptr,
                10
            );

            // Allow trailing spaces
            while (isspace((unsigned char)*endptr)) {
                endptr++;
            }

            if (errno == ERANGE || *endptr != '\0') {
                printf("Invalid history number.\n");
                free(cmdline);
                continue;
            }

            // Retrieve the requested command
            const char* previous = custom_history_get(&history, number);

            if (previous == NULL) {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }

            // Allocate enough memory for the recalled command
            char* expanded = strdup(previous);

            if (expanded == NULL) {
                perror("strdup");
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = expanded;

            printf("%s\n", cmdline);
        }

        // Store the actual command in custom history
        history_add(&history, cmdline);

        // Also store it in Readline history for arrow navigation
        add_history(cmdline);

        // Tokenize command
        arglist = tokenize(cmdline);

        if (arglist == NULL) {
            free(cmdline);
            continue;
        }

        // Check built-in commands first
        if (arglist[0] != NULL) {

            if (handle_builtin(arglist, &history) == 0) {
                execute(arglist);
            }
        }

        // Free allocated argument memory
        for (int i = 0; arglist[i] != NULL; i++) {
            free(arglist[i]);
        }

        free(arglist);
        free(cmdline);
    }

    // Clean up history memory
    for (int i = 0; i < HISTORY_SIZE; i++) {
        free(history.commands[i]);
    }

    cleanup_generator();

    return 0;
}
