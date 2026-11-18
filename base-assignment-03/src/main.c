#define _POSIX_C_SOURCE 200809L

#include "shell.h"

#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>

// Completion state
static char* path_copy = NULL;
static char* path_saveptr = NULL;
static char* dir_path = NULL;
static DIR* current_dir = NULL;

// Clean up completion resources
static void cleanup_generator(void)
{
    if (current_dir != NULL) {
        closedir(current_dir);
        current_dir = NULL;
    }

    free(path_copy);
    path_copy = NULL;

    path_saveptr = NULL;
    dir_path = NULL;
}

// Generate executable command names from PATH
static char* command_generator(const char* text, int state)
{
    struct dirent* entry;
    struct stat file_info;

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

        dir_path = strtok_r(path_copy, ":", &path_saveptr);
    }

    if (path_copy == NULL) {
        return NULL;
    }

    while (1) {
        if (current_dir == NULL) {
            if (dir_path == NULL) {
                cleanup_generator();
                return NULL;
            }

            current_dir = opendir(dir_path);
            if (current_dir == NULL) {
                dir_path = strtok_r(NULL, ":", &path_saveptr);
                continue;
            }
        }

        while ((entry = readdir(current_dir)) != NULL) {
            if (strncmp(entry->d_name, text, strlen(text)) != 0) {
                continue;
            }

            size_t size = strlen(dir_path) + strlen(entry->d_name) + 2;
            char* full_path = malloc(size);
            if (full_path == NULL) {
                continue;
            }

            snprintf(full_path, size, "%s/%s", dir_path, entry->d_name);

            int valid =
                stat(full_path, &file_info) == 0 &&
                S_ISREG(file_info.st_mode) &&
                access(full_path, X_OK) == 0;

            free(full_path);

            if (valid) {
                return strdup(entry->d_name);
            }
        }

        closedir(current_dir);
        current_dir = NULL;

        dir_path = strtok_r(NULL, ":", &path_saveptr);
    }
}

// Command and filename completion
static char** shell_completion(const char* text, int start, int end)
{
    (void)end;

    if (start == 0 && strchr(text, '/') == NULL) {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, command_generator);
    }

    return NULL;
}

// Check for empty or whitespace-only input
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
    History history = {0};
    JobList job_list;

    job_list_init(&job_list);

    // Initialize GNU Readline history
    using_history();
    stifle_history(HISTORY_SIZE);

    // Enable completion
    rl_attempted_completion_function = shell_completion;

    while (1) {
        // Reap any terminated background jobs before prompting
        pid_t reaped_pid;
        int status;
        while ((reaped_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            job_list_remove(&job_list, reaped_pid);
        }

        cmdline = readline(PROMPT);

        // Ctrl+D
        if (cmdline == NULL) {
            printf("\nExiting MyShell...\n");
            break;
        }

        if (is_blank(cmdline)) {
            free(cmdline);
            continue;
        }

        // Handle !n history expansion
        if (cmdline[0] == '!') {
            char* endptr;

            if (!isdigit((unsigned char)cmdline[1])) {
                printf("Invalid history command.\n");
                free(cmdline);
                continue;
            }

            errno = 0;
            unsigned long number = strtoul(cmdline + 1, &endptr, 10);

            while (isspace((unsigned char)*endptr)) {
                endptr++;
            }

            if (errno == ERANGE || *endptr != '\0') {
                printf("Invalid history number.\n");
                free(cmdline);
                continue;
            }

            const char* previous = custom_history_get(&history, number);
            if (previous == NULL) {
                printf("No such command in history.\n");
                free(cmdline);
                continue;
            }

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

        // Save actual command in both histories
        history_add(&history, cmdline);
        add_history(cmdline);

        // Split by semicolons for command chaining
        int num_subcmds = 0;
        char** subcmds = split_command_lines(cmdline, &num_subcmds);

        for (int i = 0; i < num_subcmds; i++) {
            Command* commands = tokenize(subcmds[i]);

            if (commands == NULL) {
                continue;
            }

            // Handle standalone built-ins without redirection or piping
            if (commands->next == NULL &&
                commands->input_file == NULL &&
                commands->output_file == NULL &&
                !commands->background) {

                if (handle_builtin(commands->argv, &history, &job_list) == 0) {
                    execute_pipeline(commands, &job_list, subcmds[i]);
                }
            } else {
                execute_pipeline(commands, &job_list, subcmds[i]);
            }

            free_commands(commands);
        }

        // Free subcommands array
        for (int i = 0; i < num_subcmds; i++) {
            free(subcmds[i]);
        }
        free(subcmds);

        free(cmdline);
    }

    // Free custom history memory
    for (int i = 0; i < HISTORY_SIZE; i++) {
        free(history.commands[i]);
    }

    cleanup_generator();
    return 0;
}