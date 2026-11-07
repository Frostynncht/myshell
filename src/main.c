#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "lexer.h"
#include "runner.h"
#include "commands.h"
#include "input.h"
#include "parser.h"

int main() {
    // игнорируем Ctrl+C в родительском процессе
    signal(SIGINT, SIG_IGN);

    // запоминаем код завершения помледней команды
    int last_status = 0;

    //запоминаем вводится ли команда с терминала
    int interactive = isatty(STDIN_FILENO);

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

        Token *tokens = split_line(line);
        free(line); // больше не нужна(лексер делает копии)

        // проверка завершения лексера
        if (tokens == NULL) {
            last_status = 2;

            if (interactive == 0) {
                return last_status;
            }

            continue;
        }

        // если нет токенов после лексера
        if (tokens[0].type == TOKEN_END) {
            free_tokens(tokens);
            continue;
        }

        // проверяем команду и строим дерево
        Node *tree = parse_tokens(tokens);
        if (tree == NULL) {
            last_status = 2;
            free_tokens(tokens);

            if (interactive == 0) {
                return last_status;
            }

            continue;
        }
        
        // пока используем дерево только для проверки
        free_tree(tree);

        // считает токены для массива arg
        size_t count = 0;
        while(tokens[count].type != TOKEN_END) {
            count++;
        }

        // создаем массив аргументов(text токена) для передачи в runner
        char **arg = malloc((count + 1) * sizeof(char *));
        if (arg == NULL) {
            perror("mysh: malloc");
            free_tokens(tokens);
            break;
        }

        for (size_t i = 0; i < count; i++) {
            arg[i] = tokens[i].text;
        }

        arg[count] = NULL;

        // проверка на наличие чего-либо кроме слов
        int have_operator = 0;

        for (size_t i = 0; i < count; i++) {
            if (tokens[i].type != TOKEN_WORD) {
                have_operator = 1;
                break;
            }
        }

        // проверка на наличие оператора пайпа
        int have_pipe = 0;

        for (size_t i = 0; i < count; i++) {
            if (tokens[i].type == TOKEN_PIPE) {
                have_pipe = 1;
                break;
            }
        }

        if (arg[0] == NULL) {
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // без пайпа exit завершает сам шелл
        if (strcmp(arg[0], "exit") == 0 && have_pipe == 0) {
            int exit_status = last_status;
            if (arg[1] != NULL) {
                if (arg[2] != NULL) {
                    fprintf(stderr, "mysh: exit: too many arguments\n");
                    last_status = 2;
                    free(arg);
                    free_tokens(tokens);
                    continue;
                }

                const char *number_text = arg[1];
                int i = 0;
                int neg = 0;
                int valid = 1;
                int number = 0;

                // пропускаем знак числа(запоминаем если минус)
                if (number_text[i] == '+' || number_text[i] == '-') {
                    if (number_text[i] == '-') {
                        neg = 1;
                    }
                    i++;
                }

                if (number_text[i] == '\0') {
                    valid = 0;
                }

                // проверяем каждый символ и собираем число
                while (number_text[i] != '\0') {
                    if (number_text[i] < '0' || number_text[i] > '9') {
                        valid = 0;
                        break;
                    }

                    // в пределах [0; 255]
                    number = (number * 10 + (number_text[i] - '0')) % 256;
                    i++;
                }

                if (valid == 0) {
                    fprintf(stderr, "mysh: exit: numeric argument required\n");
                    last_status = 2;
                    free(arg);
                    free_tokens(tokens);
                    continue;
                }

                // переводим отрицательное в положительное [0; 255]
                if (neg == 1) {
                    number = (256 - number) % 256;
                }

                exit_status = number;
            }
            
            free(arg);
            free_tokens(tokens);
            return exit_status;
        }

        // cd
        if (strcmp(arg[0], "cd") == 0 && have_pipe == 0) {
            last_status = run_cd(arg);
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // pwd
        if (strcmp(arg[0], "pwd") == 0 && have_operator == 0) {
            last_status = run_pwd();
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // исполняет остальные команды
        last_status = runner(arg, tokens);

        free(arg);
        free_tokens(tokens);
    }

    return last_status;
}