#include "shell.h"


// Apply input redirection
static int redirect_input(const char* filename)
{
    if (filename == NULL) {
        return 0;
    }

    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        perror(filename);
        return -1;
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        perror("dup2 input");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}


// Apply output redirection
static int redirect_output(const char* filename)
{
    if (filename == NULL) {
        return 0;
    }

    int fd = open(
        filename,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (fd < 0) {
        perror(filename);
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("dup2 output");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}


// Execute commands connected through pipes
int execute_pipeline(Command* commands)
{
    if (commands == NULL) {
        return -1;
    }

    // Count commands
    int count = 0;

    for (Command* temp = commands; temp != NULL; temp = temp->next) {
        count++;
    }

    pid_t* pids = calloc(count, sizeof(pid_t));

    if (pids == NULL) {
        perror("calloc");
        return -1;
    }

    int previous_read = -1;
    int started = 0;
    int failed = 0;

    Command* current = commands;

    for (int i = 0; i < count; i++) {

        int pipefd[2] = {-1, -1};

        // Create a pipe if another command follows
        if (current->next != NULL) {

            if (pipe(pipefd) < 0) {
                perror("pipe");
                failed = 1;
                break;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");

            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            failed = 1;
            break;
        }

        // Child process
        if (pid == 0) {

            // Read from previous pipe
            if (previous_read != -1) {

                if (dup2(previous_read, STDIN_FILENO) < 0) {
                    perror("dup2 pipe input");
                    _exit(EXIT_FAILURE);
                }
            }

            // Write to next pipe
            if (pipefd[1] != -1) {

                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    perror("dup2 pipe output");
                    _exit(EXIT_FAILURE);
                }
            }

            // Explicit redirection overrides pipe connections
            if (redirect_input(current->input_file) < 0) {
                _exit(EXIT_FAILURE);
            }

            if (redirect_output(current->output_file) < 0) {
                _exit(EXIT_FAILURE);
            }

            // Close unused descriptors
            if (previous_read != -1) {
                close(previous_read);
            }

            if (pipefd[0] != -1) {
                close(pipefd[0]);
            }

            if (pipefd[1] != -1) {
                close(pipefd[1]);
            }

            // Execute command
            execvp(current->argv[0], current->argv);

            perror(current->argv[0]);
            _exit(EXIT_FAILURE);
        }

        // Parent process
        pids[started++] = pid;

        // Parent no longer needs previous pipe
        if (previous_read != -1) {
            close(previous_read);
        }

        // Parent does not write into the new pipe
        if (pipefd[1] != -1) {
            close(pipefd[1]);
        }

        // Save read end for the next command
        previous_read = pipefd[0];

        current = current->next;
    }

    // Close remaining pipe descriptor
    if (previous_read != -1) {
        close(previous_read);
    }

    // Wait for every child process
    for (int i = 0; i < started; i++) {

        int status;

        while (waitpid(pids[i], &status, 0) < 0) {

            if (errno == EINTR) {
                continue;
            }

            perror("waitpid");
            failed = 1;
            break;
        }
    }

    free(pids);

    return failed ? -1 : 0;
}