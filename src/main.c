#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "lexer.h"

int main() {
    printf("myshell: shell started\n");

    char line[1024];

    while (1) {
        write(STDOUT_FILENO, "mysh$ ", 6);

        if (fgets( line, sizeof(line), stdin) == NULL) { // записывает в line ввод с клавиатуры(stdin) размером не более line(1023)
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        char **arg = split_line(line);

        if (arg[0] == NULL) {
            free_arg(arg);
            continue;
        }

        if (strcmp(arg[0], "exit") == 0) {
            free_arg(arg);
            break;
        }

        // cd
        if (strcmp(arg[0], "cd") == 0) {
            char *target_dir = arg[1];

            if (target_dir == NULL) {
                target_dir = getenv("HOME");

                if (target_dir == NULL) {
                    fprintf(stderr, "mysh: cd: HOME not set\n");
                    free_arg(arg);
                    continue;
                }
            }

            if (chdir(target_dir) != 0) {
                perror("mysh: cd");
            }

            free_arg(arg);
            continue;
        }

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
        } else if (pid == 0) {
            char *cmd_args[1024]; // массив аргументов для execvp
            int cmd_arg = 0; // счетчик аргументов

            for (int i = 0; arg[i] != NULL; i++) {
            // обработка перенаправления (ввод/вывод)
                // обработка символа >
                if (strcmp(arg[i], ">") == 0) {
                    i++;
                    // проверка на наличие имени после символа >
                    if (arg[i] == NULL) {
                        fprintf(stderr, "mysh: missing file name for redirection\n");
                        _exit(1);
                    }

                    int fd = open(arg[i], O_WRONLY | O_CREAT | O_TRUNC, 0666); // переписываем файл если он существует или создает новый
                    if (fd == -1) {
                        perror("mysh: open");
                        _exit(1);
                    }
                    // перенаправление вывода в файл
                    dup2(fd, STDOUT_FILENO);
                    close(fd); 
                
                // обработка символа >>
                } else if (strcmp(arg[i], ">>") == 0) {
                    i++;

                    if (arg[i] == NULL) {
                        fprintf(stderr, "mysh: missing file name for redirection\n");
                        _exit(1);
                    }

                    int fd = open(arg[i], O_WRONLY | O_CREAT | O_APPEND, 0666); // записываем в конец файла если он существует или создает новый
                    if (fd == -1) {
                        perror("mysh: open");
                        _exit(1);
                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);

                // обработка символа <
                } else if (strcmp(arg[i], "<") == 0) {
                    i++;

                    if (arg[i] == NULL) {
                        fprintf(stderr, "mysh: missing file name for redirection\n");
                        _exit(1);
                    }

                    int fd = open(arg[i], O_RDONLY); // открываем файл только для чтения
                    if (fd == -1) {
                        perror("mysh: open");
                        _exit(1);
                    }
                    // перенаправление ввода из файла
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                } else {
                    cmd_args[cmd_arg++] = arg[i]; // если не символ перенаправления то добавляем в массив аргументов
                }
            }

            cmd_args[cmd_arg] = NULL; // ставим NULL в конец массива аргументов

            if (cmd_arg > 0) {
                execvp(cmd_args[0], cmd_args);
                perror("mysh");
                _exit(127);
            }
            _exit(0);

        } else {
            int status;
            wait(&status);
        }

        free_arg(arg);  
    }

    return 0;
}