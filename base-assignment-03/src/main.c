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

// Count nesting depth of if/fi keywords
static int count_if_depth(const char* str)
{
    int depth = 0;
    const char* p = str;
    while (*p) {
        if ((p == str || isspace((unsigned char)*(p-1)) || *p == ';') &&
            strncmp(p, "if", 2) == 0 &&
            (isspace((unsigned char)*(p+2)) || p[2] == '\0' || p[2] == ';')) {
            depth++;
            p += 2;
        } else if ((p == str || isspace((unsigned char)*(p-1)) || *p == ';') &&
                   strncmp(p, "fi", 2) == 0 &&
                   (isspace((unsigned char)*(p+2)) || p[2] == '\0' || p[2] == ';')) {
            depth--;
            p += 2;
        } else {
            p++;
        }
    }
    return depth;
}

// Structure to hold parsed parts of an if-then-else-fi block
typedef struct {
    char* condition_str;
    char* then_str;
    char* else_str;
} IfBlock;

// Parse an if block into condition, then, and else components
static void parse_if_block(const char* block_str, IfBlock* out)
{
    out->condition_str = NULL;
    out->then_str = NULL;
    out->else_str = NULL;

    const char* p = block_str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "if", 2) == 0) p += 2;

    const char* cond_start = p;
    const char* then_pos = NULL;

    p = cond_start;
    while (*p) {
        if ((p == block_str || isspace((unsigned char)*(p-1)) || *p == ';') &&
            strncmp(p, "then", 4) == 0 &&
            (isspace((unsigned char)*(p+4)) || p[4] == '\0' || p[4] == ';')) {
            then_pos = p;
            break;
        }
        p++;
    }
    if (!then_pos) return;

    size_t cond_len = then_pos - cond_start;
    out->condition_str = malloc(cond_len + 1);
    memcpy(out->condition_str, cond_start, cond_len);
    out->condition_str[cond_len] = '\0';

    p = then_pos + 4;
    const char* then_start = p;
    const char* else_pos = NULL;
    const char* fi_pos = NULL;

    p = then_start;
    while (*p) {
        if ((p == block_str || isspace((unsigned char)*(p-1)) || *p == ';') &&
            strncmp(p, "else", 4) == 0 &&
            (isspace((unsigned char)*(p+4)) || p[4] == '\0' || p[4] == ';')) {
            else_pos = p;
            break;
        }
        if ((p == block_str || isspace((unsigned char)*(p-1)) || *p == ';') &&
            strncmp(p, "fi", 2) == 0 &&
            (isspace((unsigned char)*(p+2)) || p[2] == '\0' || p[2] == ';')) {
            fi_pos = p;
            break;
        }
        p++;
    }

    if (else_pos) {
        size_t then_len = else_pos - then_start;
        out->then_str = malloc(then_len + 1);
        memcpy(out->then_str, then_start, then_len);
        out->then_str[then_len] = '\0';

        p = else_pos + 4;
        const char* else_start = p;

        while (*p) {
            if ((p == block_str || isspace((unsigned char)*(p-1)) || *p == ';') &&
                strncmp(p, "fi", 2) == 0 &&
                (isspace((unsigned char)*(p+2)) || p[2] == '\0' || p[2] == ';')) {
                fi_pos = p;
                break;
            }
            p++;
        }
        if (fi_pos) {
            size_t else_len = fi_pos - else_start;
            out->else_str = malloc(else_len + 1);
            memcpy(out->else_str, else_start, else_len);
            out->else_str[else_len] = '\0';
        }
    } else if (fi_pos) {
        size_t then_len = fi_pos - then_start;
        out->then_str = malloc(then_len + 1);
        memcpy(out->then_str, then_start, then_len);
        out->then_str[then_len] = '\0';
    }
}

// Execute the if-then-else-fi block
static int execute_if_block(IfBlock* block, History* history, JobList* job_list)
{
    Command* cond_cmds = tokenize(block->condition_str);
    if (!cond_cmds) return -1;

    int exit_status = 0;
    if (cond_cmds->next == NULL &&
        cond_cmds->input_file == NULL &&
        cond_cmds->output_file == NULL &&
        !cond_cmds->background) {

        if (handle_builtin(cond_cmds->argv, history, job_list) == 0) {
            exit_status = execute_pipeline(cond_cmds, job_list, block->condition_str);
        }
    } else {
        exit_status = execute_pipeline(cond_cmds, job_list, block->condition_str);
    }
    free_commands(cond_cmds);

    // Choose branch: exit status 0 is success (then), non-zero is failure (else)
    char* branch_str = (exit_status == 0) ? block->then_str : block->else_str;
    if (branch_str == NULL) {
        return 0;
    }

    // Normalize newlines to semicolons so multi-line blocks work with command chaining
    char* normalized = strdup(branch_str);
    for (int i = 0; normalized[i] != '\0'; i++) {
        if (normalized[i] == '\n' || normalized[i] == '\r') {
            normalized[i] = ';';
        }
    }

    int num_subcmds = 0;
    char** subcmds = split_command_lines(normalized, &num_subcmds);
    for (int i = 0; i < num_subcmds; i++) {
        Command* cmds = tokenize(subcmds[i]);
        if (cmds == NULL) continue;

        if (cmds->next == NULL &&
            cmds->input_file == NULL &&
            cmds->output_file == NULL &&
            !cmds->background) {

            if (handle_builtin(cmds->argv, history, job_list) == 0) {
                execute_pipeline(cmds, job_list, subcmds[i]);
            }
        } else {
            execute_pipeline(cmds, job_list, subcmds[i]);
        }
        free_commands(cmds);
    }

    for (int i = 0; i < num_subcmds; i++) {
        free(subcmds[i]);
    }
    free(subcmds);
    free(normalized);

    return 0;
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

        // Check for multi-line if block
        char* trimmed = cmdline;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

        if (strncmp(trimmed, "if", 2) == 0 && (trimmed[2] == '\0' || isspace((unsigned char)trimmed[2]))) {
            int depth = count_if_depth(cmdline);
            char* full_block = cmdline;

            while (depth > 0) {
                char* next_line = readline("> ");
                if (!next_line) break;

                size_t new_len = strlen(full_block) + strlen(next_line) + 2;
                char* temp = malloc(new_len);
                sprintf(temp, "%s\n%s", full_block, next_line);
                free(full_block);
                full_block = temp;
                free(next_line);

                depth = count_if_depth(full_block);
            }

            history_add(&history, full_block);
            add_history(full_block);

            IfBlock if_block;
            parse_if_block(full_block, &if_block);

            if (if_block.condition_str && if_block.then_str) {
                execute_if_block(&if_block, &history, &job_list);
            } else {
                fprintf(stderr, "Syntax error in if-then-else block\n");
            }

            free(if_block.condition_str);
            free(if_block.then_str);
            free(if_block.else_str);
            free(full_block);
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