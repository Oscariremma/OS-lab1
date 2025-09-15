/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <fcntl.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"

static int current_foreground_group_id = -1;

static void print_cmd(Command *cmd);

static void print_pgm(Pgm *p);

void stripwhite(char *);

int exec_program(Command *cmd);

void sigchld_handler(int signum);

void sigint_handler(int signum);

void send_sighup();

int main(void) {
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);

    for (;;) {
        char *line;
        line = readline("> ");

        if (line == NULL) {
            // EOF, e.g., user pressed Ctrl-D
            printf("\n");
            free(line);
            break;
        }

        // Remove leading and trailing whitespace from the line
        stripwhite(line);

        // If stripped line not blank
        if (*line) {
            add_history(line);

            Command cmd;
            if (parse(line, &cmd) == 1) {
                // Just prints cmd
                print_cmd(&cmd);
                exec_program(&cmd);
            } else {
                printf("Parse ERROR\n");
            }
        }

        // Clear memory
        free(line);
    }

    send_sighup();

    return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list) {
    printf("------------------------------\n");
    printf("Parse OK\n");
    printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
    printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
    printf("background: %s\n", cmd_list->background ? "true" : "false");
    printf("Pgms:\n");
    print_pgm(cmd_list->pgm);
    printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p) {
    if (p == NULL) {
        return;
    } else {
        char **pl = p->pgmlist;

        /* The list is in reversed order so print
         * it reversed to get right
         */
        print_pgm(p->next);
        printf("            * [ ");
        while (*pl) {
            printf("%s ", *pl++);
        }
        printf("]\n");
    }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string) {
    size_t i = 0;

    while (isspace(string[i])) {
        i++;
    }

    if (i) {
        memmove(string, string + i, strlen(string + i) + 1);
    }

    i = strlen(string) - 1;
    while (i > 0 && isspace(string[i])) {
        i--;
    }

    string[++i] = '\0';
}

bool cd(int *input_fd, char **args, int pipefd[2]) {
    char *path = args[1] == NULL ? "~" : args[1];

    bool needs_free = false;
    if (path[0] == '~') {
        char *home = getenv("HOME");
        int len = snprintf(NULL, 0, "%s%s", home, path + 1) + 1;
        char *appended_path = malloc(len);
        needs_free = true;
        snprintf(appended_path, len, "%s%s", home, path + 1);
        path = appended_path;
    }

    if (chdir(path) != 0) {
        perror("chdir failed");
        if (needs_free)
            free(path);
        return false;
    }

    if (needs_free)
        free(path);


    if (*input_fd != -1) {
        close(*input_fd);
    }

    *input_fd = pipefd[0];
    close(pipefd[1]);
    return true;
}

void send_sighup() {
    pid_t pid = getpid();

    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%d/task/%d/children", pid, pid);
    FILE *file = fopen(buf, "r");
    if (file == NULL) {
        perror("Failed to open children file");
    } else {
        pid_t child_pid;
        while (fscanf(file, "%d", &child_pid) == 1) {
            if (kill(child_pid, SIGHUP) == -1) {
                perror("Failed to send SIGHUP");
            }
        }
        fclose(file);
    }
}

int exec_program(Command *cmd) {
    if (cmd == NULL)
        return 0;

    Pgm *pgm_list = cmd->pgm;

    int count = 0;
    Pgm *pgm_iter = pgm_list;
    while (pgm_iter) {
        count++;
        pgm_iter = pgm_iter->next;
    }

    // Commands in right to left order
    Pgm *commands[count];

    pid_t children[count];

    pid_t group_id = -1;

    pgm_iter = pgm_list;
    for (int i = count - 1; i >= 0; i--) {
        commands[i] = pgm_iter;
        pgm_iter = pgm_iter->next;
    }

    int input_fd = -1;

    if (cmd->rstdin) {
        input_fd = open(cmd->rstdin, O_RDONLY);
        if (input_fd < 0) {
            perror("Failed to open input file");
            return -1;
        }
    }

    for (int i = 0; i < count; i++) {
        Pgm *pgm = commands[i];
        const char *program = pgm->pgmlist[0];
        char **args = pgm->pgmlist;

        int pipefd[2] = {-1, -1};
        bool has_next = (i < count - 1);

        if (has_next) {
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                exit(-1);
            }
        }

        if (strcmp(program, "cd") == 0) {
            if (!cd(&input_fd, args, pipefd))
                return -1;
            children[i] = 0; // No pid for cd
            continue;
        }

        if (strcmp(program, "exit") == 0) {
            send_sighup();
            exit(0);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(-1);
        } else if (pid == 0) {
            // Child process

            if (input_fd != -1) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            if (has_next) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);
            } else if (cmd->rstdout) {
                int output_fd = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd < 0) {
                    perror("Failed to open output file");
                    exit(-1);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            execvp(program, args);
            perror("execvp failed");
            exit(-1);
        } else {
            // Parent process

            children[i] = pid;

            if (group_id == -1)
                group_id = pid;

            if (setpgid(pid, group_id)) {
                perror("setpgid failed");
                exit(-1);
            }

            if (input_fd != -1) {
                close(input_fd);
            }

            if (has_next) {
                close(pipefd[1]);
                input_fd = pipefd[0];
            }
        }
    }

    if (input_fd != -1) {
        close(input_fd);
    }

    if (!cmd->background) {
        current_foreground_group_id = group_id;
        for (int i = 0; i < count; i++) {
            if (children[i] != 0) {
                waitpid(children[i], NULL, 0);
            }
        }
        current_foreground_group_id = -1;
    }

    return 0;
}

void sigchld_handler(int signum) {
    if (signum != SIGCHLD) {
        return;
    }
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void sigint_handler(int signum) {
    if (signum != SIGINT) {
        return;
    }
    if (current_foreground_group_id != -1) {
        kill(-current_foreground_group_id, SIGINT);
    }
}
