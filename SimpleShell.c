#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 128

void parse_command(char *cmd, char **argv) {
    while (*cmd != '\0') {
        while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n') {
            *cmd++ = '\0';
        }
        *argv++ = cmd;
        while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n') {
            cmd++;
        }
    }
    *argv = '\0';
}

void execute_command(char **argv) {
    pid_t pid;
    int status;

    if ((pid = fork()) < 0) {
        perror("Forking child process failed");
        exit(1);
    } else if (pid == 0) {  // Child process
        if (execvp(*argv, argv) < 0) {
            perror("Exec failed");
            exit(1);
        }
    } else {  // Parent process
        while (wait(&status) != pid);
    }
}

int main() {
    char cmd[MAX_LINE];
    char *argv[MAX_ARGS];
    int should_run = 1;

    while (should_run) {
        printf("myshell> ");
        fgets(cmd, MAX_LINE, stdin);

        if (cmd[strlen(cmd) - 1] == '\n') {
            cmd[strlen(cmd) - 1] = '\0';
        }

        parse_command(cmd, argv);

        if (strcmp(argv[0], "exit") == 0) {
            should_run = 0;
        } else if (strcmp(argv[0], "pwd") == 0) {
            char cwd[MAX_LINE];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
            }
        } else if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: expected argument to \"cd\"\n");
            } else {
                if (chdir(argv[1]) != 0) {
                    perror("cd failed");
                }
            }
        } else {
            int i = 0;
            int fd;
            int is_redirect = 0;
            int is_pipe = 0;
            char *cmd1[MAX_ARGS];
            char *cmd2[MAX_ARGS];

            while (argv[i] != NULL) {
                if (strcmp(argv[i], ">") == 0) {
                    is_redirect = 1;
                    argv[i] = NULL;
                    cmd1[i] = NULL;
                    fd = open(argv[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    break;
                } else if (strcmp(argv[i], "|") == 0) {
                    is_pipe = 1;
                    argv[i] = NULL;
                    cmd1[i] = NULL;
                    int j = 0;
                    while (argv[i + 1 + j] != NULL) {
                        cmd2[j] = argv[i + 1 + j];
                        j++;
                    }
                    cmd2[j] = NULL;
                    break;
                }
                cmd1[i] = argv[i];
                i++;
            }

            if (is_redirect) {
                if (fork() == 0) {
                    dup2(fd, 1);
                    close(fd);
                    execvp(cmd1[0], cmd1);
                    perror("Exec failed");
                }
                close(fd);
                wait(NULL);
            } else if (is_pipe) {
                int pipefd[2];
                pipe(pipefd);

                if (fork() == 0) {
                    dup2(pipefd[1], 1);
                    close(pipefd[0]);
                    close(pipefd[1]);
                    execvp(cmd1[0], cmd1);
                    perror("Exec failed");
                }

                if (fork() == 0) {
                    dup2(pipefd[0], 0);
                    close(pipefd[0]);
                    close(pipefd[1]);
                    execvp(cmd2[0], cmd2);
                    perror("Exec failed");
                }

                close(pipefd[0]);
                close(pipefd[1]);
                wait(NULL);
                wait(NULL);
            } else {
                execute_command(argv);
            }
        }
    }
    return 0;
} 
