#include "shell.h"

// Variable storage helper functions
void var_set(VarNode** head, const char* name, const char* value)
{
    VarNode* current = *head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            free(current->value);
            current->value = strdup(value ? value : "");
            return;
        }
        current = current->next;
    }

    VarNode* new_node = malloc(sizeof(VarNode));
    if (new_node == NULL) {
        perror("malloc");
        return;
    }
    new_node->name = strdup(name);
    new_node->value = strdup(value ? value : "");
    new_node->next = *head;
    *head = new_node;
}

const char* var_get(VarNode* head, const char* name)
{
    VarNode* current = head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

void var_free(VarNode* head)
{
    while (head != NULL) {
        VarNode* next = head->next;
        free(head->name);
        free(head->value);
        free(head);
        head = next;
    }
}

// Check if a string is a variable assignment (VAR=value with no spaces around =)
int is_assignment(const char* str, char** name_out, char** val_out)
{
    if (str == NULL) return 0;
    
    const char* eq = strchr(str, '=');
    if (eq == NULL || eq == str) return 0;

    for (const char* p = str; p < eq; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            return 0;
        }
    }

    if (name_out) *name_out = strndup(str, eq - str);
    if (val_out) *val_out = strdup(eq + 1);
    return 1;
}

// Expand variables anywhere within a string
static char* expand_string(const char* str, VarNode* vars)
{
    if (str == NULL) return NULL;

    size_t len = strlen(str);
    size_t cap = len * 2 + 64;
    char* result = malloc(cap);
    if (result == NULL) return NULL;
    result[0] = '\0';

    size_t r_idx = 0;
    const char* p = str;

    while (*p != '\0') {
        if (*p == '$' && *(p + 1) != '\0' && (isalnum((unsigned char)*(p + 1)) || *(p + 1) == '_')) {
            p++; // skip '$'
            const char* name_start = p;
            while (*p != '\0' && (isalnum((unsigned char)*p) || *p == '_')) {
                p++;
            }
            size_t name_len = p - name_start;
            char* vname = strndup(name_start, name_len);

            const char* val = var_get(vars, vname);
            if (val == NULL) {
                val = getenv(vname);
            }
            if (val == NULL) {
                val = "";
            }
            free(vname);

            size_t v_len = strlen(val);
            if (r_idx + v_len >= cap) {
                cap = r_idx + v_len + 64;
                char* temp = realloc(result, cap);
                if (temp == NULL) {
                    free(result);
                    return NULL;
                }
                result = temp;
            }
            strcpy(result + r_idx, val);
            r_idx += v_len;
        } else {
            if (r_idx + 1 >= cap) {
                cap += 64;
                char* temp = realloc(result, cap);
                if (temp == NULL) {
                    free(result);
                    return NULL;
                }
                result = temp;
            }
            result[r_idx++] = *p;
            result[r_idx] = '\0';
            p++;
        }
    }
    result[r_idx] = '\0';
    return result;
}

// Expand variables in command arguments and redirection files
void expand_command_variables(Command* commands, VarNode* vars)
{
    Command* cmd = commands;
    while (cmd != NULL) {
        for (int i = 0; i < cmd->argc; i++) {
            if (cmd->argv[i] != NULL) {
                char* expanded = expand_string(cmd->argv[i], vars);
                if (expanded != NULL) {
                    free(cmd->argv[i]);
                    cmd->argv[i] = expanded;
                }
            }
        }

        if (cmd->input_file != NULL) {
            char* expanded = expand_string(cmd->input_file, vars);
            if (expanded != NULL) {
                free(cmd->input_file);
                cmd->input_file = expanded;
            }
        }

        if (cmd->output_file != NULL) {
            char* expanded = expand_string(cmd->output_file, vars);
            if (expanded != NULL) {
                free(cmd->output_file);
                cmd->output_file = expanded;
            }
        }

        cmd = cmd->next;
    }
}

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

        while (isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '&') {
            const char* next_p = p + 1;
            while (*next_p && isspace((unsigned char)*next_p)) next_p++;
            
            if (*next_p == '\0') {
                Command* last = head;
                while (last->next != NULL) {
                    last = last->next;
                }
                last->background = 1;
                p++;
                break;
            } else {
                fprintf(stderr, "Syntax error near '&'\n");
                free_commands(head);
                return NULL;
            }
        }

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