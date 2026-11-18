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
int execute_pipeline(Command* commands, JobList* job_list, const char* original_cmd)
{
    if (commands == NULL) {
        return -1;
    }

    // Check if background execution is requested (stored on the last command node)
    Command* last_cmd = commands;
    while (last_cmd->next != NULL) {
        last_cmd = last_cmd->next;
    }
    int is_background = last_cmd->background;

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
            if (previous_read != -1) {
                if (dup2(previous_read, STDIN_FILENO) < 0) {
                    perror("dup2 pipe input");
                    _exit(EXIT_FAILURE);
                }
            }

            if (pipefd[1] != -1) {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    perror("dup2 pipe output");
                    _exit(EXIT_FAILURE);
                }
            }

            if (redirect_input(current->input_file) < 0) {
                _exit(EXIT_FAILURE);
            }

            if (redirect_output(current->output_file) < 0) {
                _exit(EXIT_FAILURE);
            }

            if (previous_read != -1) close(previous_read);
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            execvp(current->argv[0], current->argv);
            perror(current->argv[0]);
            _exit(EXIT_FAILURE);
        }

        // Parent process
        pids[started++] = pid;

        if (previous_read != -1) close(previous_read);
        if (pipefd[1] != -1) close(pipefd[1]);

        previous_read = pipefd[0];
        current = current->next;
    }

    if (previous_read != -1) {
        close(previous_read);
    }

    if (failed) {
        free(pids);
        return -1;
    }

    // If background execution, do not wait; add to job list
    if (is_background) {
        // Use the last process in the pipeline as the job identifier PID
        pid_t bg_pid = pids[started - 1];
        job_list_add(job_list, bg_pid, original_cmd);
        printf("[%d] %d\n", job_list->jobs[job_list->count - 1].job_id, (int)bg_pid);
        free(pids);
        return 0;
    }

    // Otherwise, wait for every child process in the pipeline
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