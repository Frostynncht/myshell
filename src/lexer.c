#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

char **split_line(char *line) {
    size_t bufsize = 8; // кол-во слов
    size_t position = 0;

    // выделяет память под слова
    char **tokens = malloc(bufsize * sizeof(char *));
    if (tokens == NULL) {
        perror("mysh: malloc error");
        exit(1);
    }

    char *ptr = line;

    while (*ptr != '\0') {
        while(*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }

        if (*ptr == '\0') {
            break;
        }

        // запоминаем адрес слов
        tokens[position] = ptr;
        position++;

        // перевыделяем память если слов больше чем хватает памяти
        if (position >= bufsize) {
            bufsize *= 2;
            char **new_tokens = realloc(tokens, bufsize * sizeof(char *));
            if (new_tokens == NULL) {
                free(tokens);
                perror("mysh: realloc error");
                exit(1);
            }

            tokens = new_tokens;
        }

        // идем до конца слова
        while(*ptr != '\0' && *ptr != ' ' && *ptr != '\t') {
            ptr++;
        }

        // если дошли до конца слова то отделяем его от строки
        if (*ptr != '\0') {
            *ptr = '\0';
            ptr++;
        }
    }

    tokens[position] = NULL; // даем понять когда заканчиваются слова
    return tokens;
}

void free_arg(char **arg) {
    if (arg) {
        free(arg);
    }
}