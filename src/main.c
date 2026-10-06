#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "lexer.h"
#include "runner.h"

int main() {
    printf("myshell: shell started\n");

    // игнорируем Ctrl+C в родительском процессе
    signal(SIGINT, SIG_IGN);

    char line[1024];

    while (1) {
        write(STDOUT_FILENO, "mysh$ ", 6);

        if (fgets(line, sizeof(line), stdin) == NULL) { // записывает в line ввод с клавиатуры(stdin) размером не более line(1023)
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
            
            // если аргумент не указан, то переходим в домашнюю директорию
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

        // pwd
        if (strcmp(arg[0], "pwd") == 0) {
            char pwd[1024];
        
            if (getcwd(pwd, sizeof(pwd)) != NULL) {
                printf("%s\n", pwd);
            } else {
                perror("mysh: pwd");
            }
            free_arg(arg);
            continue;
        }

        // исполняем команду
        runner(arg);

        free_arg(arg);
    }

    return 0;
}