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
#include <readline/readline.h>
#include <readline/history.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <stdbool.h>
#include <unistd.h>

#include "parse.h"

static void print_cmd(Command *cmd);

static void print_pgm(Pgm *p);

void stripwhite(char *);

int exec_program(Pgm* next_pgm, bool background);

void sigchld_handler(int signum);

int main(void) {

    signal(SIGCHLD, sigchld_handler);

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
                exec_program(cmd.pgm, cmd.background);
            } else {
                printf("Parse ERROR\n");
            }
        }

        // Clear memory
        free(line);
    }

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


int exec_program(Pgm * next_pgm, bool background) {

    int count = 0;
    Pgm *pgm_iter = next_pgm;
    while (pgm_iter) {
        count++;
        pgm_iter = pgm_iter->next;
    }

    int child_index = 0;
    pid_t children[count];
    int curr_stdout = STDOUT_FILENO;

    while (next_pgm) {
        Pgm *curr_pgm = next_pgm;
        next_pgm = curr_pgm->next;

        const char *program = curr_pgm->pgmlist[0];
        char **args = curr_pgm->pgmlist;

        int pipefd[2];
        if (next_pgm) {
            pipe(pipefd);
        }


        const pid_t pid = fork();

        if (pid < 0) {
            assert(false);
        } else if (pid == 0) {
            if (next_pgm) {
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
            }
            dup2(curr_stdout, STDOUT_FILENO);
            execvp(program, args);
            perror("execvp failed");
            exit(-1);
        } else {
            children[child_index++] = pid;
            if (next_pgm) {
                close(pipefd[0]);
                if (curr_stdout != STDOUT_FILENO) {
                    close(curr_stdout);
                }
                curr_stdout = pipefd[1];
            }
        }
    }

    if (!background) {
        for (int i = 0; i < child_index; i++) {
            waitpid(children[i], NULL, 0);
        }
    }


    return 0;
}

void sigchld_handler(int signum) {
    if (signum != SIGCHLD) {
        return;
    }
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
