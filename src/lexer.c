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

        // если это не оператор, то ищем слово
        if (type == TOKEN_WORD) { 
            char *word = malloc(strlen(ptr) + 1);
            if (word == NULL) {
                perror("mysh: malloc");
                exit(1);
            }

            // обработка кавычек
            size_t word_len = 0;
            char quote = '\0';

            while (*ptr != '\0') {
                // обработка \ внутри кавычек и вне их
                if (*ptr == '\\' && quote != '\'') {
                    if (quote == '\0' && ptr[1] == '\0') {
                        fprintf(stderr, "mysh: unfinished slash\n");
                        free(word);
                        tokens[position].type = TOKEN_END;
                        tokens[position].text = NULL;
                        free_tokens(tokens);
                        return NULL;
                    }

                    // если мы не в кавычках и после \ стоит " или \, то печатаем их
                    if (quote == '\0' || ptr[1] == '"' || ptr[1] == '\\') {
                        // пропускаем \ и записываем следующий символ в word
                        ptr++;
                        word[word_len] = *ptr;
                        word_len++;
                        ptr++;
                        continue;
                    }
                }

                // если мы не в кавычках
                if (quote == '\0') {
                    if (*ptr == ' ' || *ptr == '\t' || strchr("|&;()<>", *ptr) != NULL) {
                        break;
                    }

                    // запоминаем кавычку и продолжаем
                    if (*ptr == '\'' || *ptr == '"') {
                        quote = *ptr;
                        ptr++;
                        continue;
                    }
                    // если внутри кавычек и встретили закрываюшую кавычку
                } else if (*ptr == quote) {
                    quote = '\0';
                    ptr++;
                    continue;
                }

                // записываем без кавычек по символьно в word
                word[word_len] = *ptr;
                word_len++;
                ptr++;
            }

            if (quote != '\0') {
                fprintf(stderr, "mysh: unclosed quote\n");
                free(word);

                // при не закрытой кавычке освобождаем уже обработанные токены
                tokens[position].type = TOKEN_END;
                tokens[position].text = NULL;
                free_tokens(tokens);
                return NULL;
            }

            word[word_len] = '\0';
            tokens[position].text = word;
        } else {
            // операторы копируем в текст токена
            tokens[position].text = strndup(start, len);
            if (tokens[position].text == NULL) {
                perror("mysh: strndup");
                exit(1);
            }
            ptr += len;
        }

        tokens[position].type = type;
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