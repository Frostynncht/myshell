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

        Token *tokens = split_line(line);
        free(line); // больше не нужна(лексер делает копии)

        size_t count = 0;

        // проверка на наличие не реализованных операторов
        while (tokens[count].type != TOKEN_END) {
            TokenType type = tokens[count].type;

            if (type == TOKEN_OR || type == TOKEN_AMPERSAND || type == TOKEN_AND || type == TOKEN_SEMICOLON || type == TOKEN_LPAREN || type == TOKEN_RPAREN) {
                fprintf(stderr, "mysh: not ready %s\n", tokens[count].text);
                break;
            }
            count++; 
        }

        if (tokens[count].type != TOKEN_END) {
            free_tokens(tokens);
            continue;
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

        if (arg[0] == NULL) {
            free(arg);
            free_tokens(tokens);
            continue;
        }

        if (strcmp(arg[0], "exit") == 0) {
            free(arg);
            free_tokens(tokens);
            break;
        }

        // cd
        if (strcmp(arg[0], "cd") == 0) {
            run_cd(arg);
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // pwd
        if (strcmp(arg[0], "pwd") == 0) {
            run_pwd();
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // исполняет остальные команды
        runner(arg, tokens);

        free(arg);
        free_tokens(tokens);
    }

    return 0;
}