#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "lexer.h"
#include "runner.h"
#include "commands.h"
#include "input.h"

int main() {
    // игнорируем Ctrl+C в родительском процессе
    signal(SIGINT, SIG_IGN);

    while (1) {
        // если не удалось вывести приглашение - завершаем
        if (print_prompt() == 0) {
            break;
        }
        
        char *line = read_line();

        // после Ctrl+D в терминале переходим на новую строку
        if (line == NULL) {
            if (isatty(STDIN_FILENO) == 1) {
                write(STDOUT_FILENO, "\n", 1);
            }
            break;
        }

        if (line[0] == '\0') {
            free(line);
            continue;
        }

        char **arg = split_line(line);
        free(line); // больше не нужна(лексер делает копии)

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