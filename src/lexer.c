#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

Token *split_line(const char *line) {
    size_t capacity = 4; // кол-во слов
    size_t position = 0;

    // выделяет память под слова
    Token *tokens = malloc(capacity * sizeof(Token));
    if (tokens == NULL) {
        perror("mysh: malloc");
        exit(1);
    }

    const char *ptr = line;

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

        // обработка комментариев
        if (*ptr == '#') {
            break;
        }

        // перевыделяем память если не хватает памяти под слова
        if (position >= capacity - 1) {
            capacity *= 2;
            Token *new_tokens = realloc(tokens, capacity * sizeof(Token));
            
            if (new_tokens == NULL) {
                perror("mysh: realloc");
                exit(1);
            }

            tokens = new_tokens;
        }

        const char *start = ptr;
        size_t len = 1;
        TokenType type = TOKEN_WORD;

        // проверка на операторы(сначала двойные)
        switch (*ptr) {
            case '|':
                if (ptr[1] == '|') {
                    type = TOKEN_OR;
                    len = 2;
                } else {
                    type = TOKEN_PIPE;
                }
                break;
            
            case '&':
                if (ptr[1] == '&') {
                    type = TOKEN_AND;
                    len = 2;
                } else {
                    type = TOKEN_AMPERSAND;
                }
                break;
            
            case '>':
                if (ptr[1] == '>') {
                    type = TOKEN_APPEND;
                    len = 2;
                } else {
                    type = TOKEN_OUTPUT;
                }
                break;

            case '<':
                type = TOKEN_INPUT;
                break;

            case ';':
                type = TOKEN_SEMICOLON;
                break;

            case '(':
                type = TOKEN_LPAREN;
                break;

            case ')':
                type = TOKEN_RPAREN;
                break;

            default:
                break;
        }

        // идем до конца слова(или еще не реализованного символа)
        if (type == TOKEN_WORD) { 
            while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && strchr("|&;()<>", *ptr) == NULL) {
                ptr++;
            }

            len = (size_t)(ptr - start);
        } else {
            ptr += len;
        }

        // обновляем информацию для токена
        tokens[position].type = type;
        tokens[position].text = strndup(start, len);

        if (tokens[position].text == NULL) {
            perror("mysh: strndup");
            exit(1);
        }

        position++;

    }

    tokens[position].type = TOKEN_END;
    tokens[position].text = NULL;
    return tokens;
}

void free_tokens(Token *tokens) {
    if (tokens == NULL) {
        return;
    }

    for (size_t i = 0; tokens[i].type != TOKEN_END; i++) {
        free(tokens[i].text);
    }

    free(tokens);
}