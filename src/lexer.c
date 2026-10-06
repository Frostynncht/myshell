#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

char **split_line(char *line) {
    size_t bufsize = 4; // кол-во слов
    size_t position = 0;

    // выделяет память под слова
    char **tokens = malloc(bufsize * sizeof(char *));
    if (tokens == NULL) {
        perror("mysh: malloc error");
        exit(1);
    }

    char *ptr = line;

    // разбиваем строку на аргументы
    while (*ptr != '\0') {
        // пропуск пробелов и табуляции
        while(*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }

        // если дошли до конца строки
        if (*ptr == '\0') {
            break;
        }

        // комментарии(все после # отбрасываем)
        if (*ptr == '#') {
            break;
        }

        // перевыделяем память если не хватает памяти под слова
        if (position >= bufsize - 1) {
            bufsize *= 2;
            char **new_tokens = realloc(tokens, bufsize * sizeof(char *));
            
            if (new_tokens == NULL) {
                free(tokens);
                perror("mysh: realloc error");
                exit(1);
            }
            tokens = new_tokens;
        }
        
        // обработка |
        if (*ptr == '|') {
            tokens[position++] = strdup("|");
            ptr += 1;
            continue;
        }

        // оператор >>
        if (*ptr == '>' && *(ptr + 1) == '>') {
            tokens[position++] = strdup(">>");     
            ptr += 2;
            continue;
        }

        // оператор >
        if (*ptr == '>') {
            tokens[position++] = strdup(">");     
            ptr += 1;
            continue;
        }

        // оператор <
        if (*ptr == '<') {
            tokens[position++] = strdup("<");  
            ptr += 1;
            continue;
        }

        char *start = ptr;
        // идем до конца слова
        while(*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '<' && *ptr != '>' && *ptr != '#' && *ptr != '|') {
            ptr++;
        }

        // выделяем память под слово и копируем его в массив слов    
        size_t word_len = ptr - start;
        char *word = malloc(word_len + 1);
        
        if (word == NULL) {
            perror("mysh: malloc error");
            exit(1);
        }

        strncpy(word, start, word_len);
        word[word_len] = '\0';
        tokens[position++] = word;
    }

    // даем понять когда конец массива слов
    tokens[position] = NULL;
    return tokens;
}

void free_arg(char **arg) {
    if (arg == NULL) {
        return;
    } else {
        for (int i = 0; arg[i] != NULL; i++) {
            free(arg[i]);
        }
        free(arg);
    }
}