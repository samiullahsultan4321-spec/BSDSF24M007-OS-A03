#include "shell.h"

// Add an argument to a command
static int add_argument(Command* command, char* argument)
{
    char** temp = realloc(
        command->argv,
        sizeof(char*) * (command->argc + 2)
    );

    if (temp == NULL) {
        perror("realloc");
        return -1;
    }

    command->argv = temp;
    command->argv[command->argc] = argument;
    command->argc++;

    command->argv[command->argc] = NULL;

    return 0;
}

// Create a new command
static Command* create_command(void)
{
    Command* command = calloc(1, sizeof(Command));

    if (command == NULL) {
        perror("calloc");
        return NULL;
    }

    return command;
}

// Free all parsed commands
void free_commands(Command* commands)
{
    while (commands != NULL) {

        Command* next = commands->next;

        for (int i = 0; i < commands->argc; i++) {
            free(commands->argv[i]);
        }

        free(commands->argv);
        free(commands->input_file);
        free(commands->output_file);
        free(commands);

        commands = next;
    }
}

// Split command line by semicolon (;), respecting quotes
char** split_command_lines(char* cmdline, int* num_cmds)
{
    if (cmdline == NULL) {
        *num_cmds = 0;
        return NULL;
    }

    int capacity = 10;
    char** cmds = malloc(sizeof(char*) * capacity);
    if (cmds == NULL) {
        perror("malloc");
        *num_cmds = 0;
        return NULL;
    }

    int count = 0;
    size_t len = strlen(cmdline);
    char* current_cmd = malloc(len + 1);
    if (current_cmd == NULL) {
        perror("malloc");
        free(cmds);
        *num_cmds = 0;
        return NULL;
    }

    size_t idx = 0;
    char quote = '\0';
    int i = 0;

    while (cmdline[i] != '\0') {
        char c = cmdline[i];

        if (quote == '\0' && (c == '\'' || c == '"')) {
            quote = c;
            current_cmd[idx++] = c;
        } else if (quote != '\0' && c == quote) {
            quote = '\0';
            current_cmd[idx++] = c;
        } else if (quote == '\0' && c == ';') {
            current_cmd[idx] = '\0';
            
            // Trim leading/trailing whitespace
            char* start = current_cmd;
            while (*start && isspace((unsigned char)*start)) start++;
            char* end = start + strlen(start) - 1;
            while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

            if (*start != '\0') {
                if (count >= capacity) {
                    capacity *= 2;
                    char** temp = realloc(cmds, sizeof(char*) * capacity);
                    if (temp == NULL) {
                        perror("realloc");
                        break;
                    }
                    cmds = temp;
                }
                cmds[count++] = strdup(start);
            }

            idx = 0;
        } else {
            current_cmd[idx++] = c;
        }
        i++;
    }

    current_cmd[idx] = '\0';
    char* start = current_cmd;
    while (*start && isspace((unsigned char)*start)) start++;
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

    if (*start != '\0') {
        if (count >= capacity) {
            char** temp = realloc(cmds, sizeof(char*) * (count + 1));
            if (temp != NULL) {
                cmds = temp;
            }
        }
        cmds[count++] = strdup(start);
    }

    free(current_cmd);
    *num_cmds = count;
    return cmds;
}

// Tokenize and parse the command line
Command* tokenize(char* cmdline)
{
    if (cmdline == NULL) {
        return NULL;
    }

    Command* head = create_command();
    if (head == NULL) {
        return NULL;
    }

    Command* current = head;
    char pending_redirection = '\0';
    const char* p = cmdline;

    while (*p != '\0') {

        // Skip whitespace
        while (isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        // Handle background operator &
        if (*p == '&') {
            // Check if & is at the end or followed by whitespace
            const char* next_p = p + 1;
            while (*next_p && isspace((unsigned char)*next_p)) next_p++;
            
            if (*next_p == '\0') {
                // Set background flag on the final command in pipeline
                Command* last = head;
                while (last->next != NULL) {
                    last = last->next;
                }
                last->background = 1;
                p++;
                break; // & terminates parsing for this line
            } else {
                fprintf(stderr, "Syntax error near '&'\n");
                free_commands(head);
                return NULL;
            }
        }

        // Handle special operators
        if (*p == '<' || *p == '>') {

            if (pending_redirection != '\0') {
                fprintf(stderr, "Syntax error: missing filename\n");
                free_commands(head);
                return NULL;
            }

            if (*p == '<' && current->input_file != NULL) {
                fprintf(stderr, "Syntax error: duplicate input redirection\n");
                free_commands(head);
                return NULL;
            }

            if (*p == '>' && current->output_file != NULL) {
                fprintf(stderr, "Syntax error: duplicate output redirection\n");
                free_commands(head);
                return NULL;
            }

            pending_redirection = *p;
            p++;
            continue;
        }

        // Handle pipe
        if (*p == '|') {

            if (pending_redirection != '\0' || current->argc == 0) {
                fprintf(stderr, "Syntax error near pipe\n");
                free_commands(head);
                return NULL;
            }

            current->next = create_command();
            if (current->next == NULL) {
                free_commands(head);
                return NULL;
            }

            current = current->next;
            p++;
            continue;
        }

        // Parse a word, preserving quoted content
        size_t capacity = strlen(p) + 1;
        char* word = malloc(capacity);

        if (word == NULL) {
            perror("malloc");
            free_commands(head);
            return NULL;
        }

        size_t i = 0;
        char quote = '\0';
        int started = 0;

        while (*p != '\0') {

            if (quote == '\0' &&
                (isspace((unsigned char)*p) ||
                 *p == '<' || *p == '>' || *p == '|' || *p == '&')) {
                break;
            }

            if (*p == '\\' && quote != '\'') {
                if (*(p + 1) != '\0') {
                    word[i++] = *(p + 1);
                    p += 2;
                    started = 1;
                    continue;
                }
            }

            if (*p == '\'' || *p == '"') {
                if (quote == '\0') {
                    quote = *p;
                    started = 1;
                    p++;
                    continue;
                }

                if (quote == *p) {
                    quote = '\0';
                    p++;
                    continue;
                }
            }

            word[i++] = *p;
            p++;
            started = 1;
        }

        if (quote != '\0') {
            fprintf(stderr, "Syntax error: unmatched quotation mark\n");
            free(word);
            free_commands(head);
            return NULL;
        }

        word[i] = '\0';

        if (!started) {
            free(word);
            continue;
        }

        // Assign the word to the pending redirection
        if (pending_redirection == '<') {
            current->input_file = word;
            pending_redirection = '\0';
        }
        else if (pending_redirection == '>') {
            current->output_file = word;
            pending_redirection = '\0';
        }
        else {
            if (add_argument(current, word) != 0) {
                free(word);
                free_commands(head);
                return NULL;
            }
        }
    }

    if (pending_redirection != '\0') {
        fprintf(stderr, "Syntax error: missing redirection filename\n");
        free_commands(head);
        return NULL;
    }

    if (current->argc == 0) {
        fprintf(stderr, "Syntax error: missing command\n");
        free_commands(head);
        return NULL;
    }

    return head;
}