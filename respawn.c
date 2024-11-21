// Program to respawn a process indefinitely until a termination signal is caught.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>


pid_t child_pid = -1;
volatile sig_atomic_t terminate = 0;
char *program_name;
char **program_args;


void print_program() {
    printf("Starting: %s", program_name);
        for (char **arg = program_args; *arg != NULL; ++arg) {
            printf(" %s", *arg);
        }
    printf("\n");
}


void start_child_process() {
    child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (child_pid == 0) {
        // Child process
        //print_program();
        execv(program_name, program_args);
        perror("execve failed");
        exit(EXIT_FAILURE);
    }
}


void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    // Wait for any child process to change state
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (terminate) {
                printf("Child process %d terminated, exiting...\n", pid);
                exit(EXIT_SUCCESS);
            } else {
                printf("Child process %d terminated, restarting...\n", pid);
                start_child_process();
            }
        }
    }
}

void forward_signal(int sig) {
    terminate = 1;
    if (child_pid > 0) {
        kill(child_pid, sig);
    }
}

void setup_signal_handlers() {
    struct sigaction sa;

    // Handle SIGCHLD
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Handle SIGINT, SIGTERM, and other termination signals
    sa.sa_handler = forward_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || 
        sigaction(SIGTERM, &sa, NULL) == -1 || 
        sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

char* find_program_in_path(const char *program) {
    if (strchr(program, '/')) {
        return strdup(program); // Program name contains a slash, use it as is
    }

    char *path = getenv("PATH");
    if (!path) {
        return NULL;
    }

    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");
    while (dir) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, program);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    program_name = find_program_in_path(argv[1]);
    if (!program_name) {
        fprintf(stderr, "Program %s not found in PATH\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    program_args = &argv[1];
    program_args[0] = program_name; // Update argv[0] to the full path

    // Set up signal handlers
    setup_signal_handlers();

    // Fork and exec the initial child process
    start_child_process();

    // Parent process waits for signals
    while (1) {
        pause();
    }

    return 0;
}
