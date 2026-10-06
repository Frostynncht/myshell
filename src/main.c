#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "lexer.h"
#include "runner.h"
#include "commands.h"

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
            run_cd(arg);
            free_arg(arg);
            continue;
        }

        // pwd
        if (strcmp(arg[0], "pwd") == 0) {
            run_pwd();

            free_arg(arg);
            continue;
        }

        // исполняет остальные команды
        runner(arg);

        free_arg(arg);
    }

    return 0;
}