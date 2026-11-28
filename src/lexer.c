#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

// определяет нужно ли читать после Enter
LineState get_line_state(const char *line) {
    const char *ptr = line;
    char quote = '\0';
    // для определения комментария в начале слова
    int word_start = 1;

    while(*ptr != '\0') {
        // в одинарных кавычках ищем только закрывающую
        if (quote == '\'') {
            if (*ptr == '\'') {
                quote = '\0';
            }

            ptr++;
            continue;
        }

        if (*ptr == '\\') {
            if (ptr[1] == '\0') {
                return LINE_SLASH;
            }

            // если мы не в кавычках или после \ стоит " или \ то пропускаем их
            if (quote == '\0' || ptr[1] == '"' || ptr[1] == '\\') {
                ptr += 2;
                word_start = 0; // слово началось
                continue;
            }
        }

        if (quote == '"') {
            if (*ptr == '"') {
                quote = '\0';
            }
        } else if (*ptr == '#' && word_start == 1) { // комментарий не влияет на продолжение команды
            return LINE_READY;
        } else if (*ptr == '\'' || *ptr == '"') { // открывающие кавычки
            quote = *ptr;
            word_start = 0; // слово началось 
        } else if (*ptr == ' ' || *ptr == '\t' || 
        strchr("|&;()<>", *ptr) != NULL) {
            word_start = 1; // слово может начаться
        } else { // обычные символы
            word_start = 0; // слово началось
        }

        ptr++;
    }

    if (quote != '\0') { // если осталась незакрытая кавычка
        return LINE_QUOTE;
    }

    return LINE_READY;
}

// возвращает массив токенов
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
            // проверяем номер дескриптора перед < или >
            const char *number_ptr = ptr;

            while(*number_ptr >= '0' && *number_ptr <= '9') {
                number_ptr++;
            }

            if (number_ptr != ptr && 
                (*number_ptr == '<' || *number_ptr == '>')) {
                fprintf(stderr, "mysh: redirection with descriptor not supported\n");
            
                // помечаем конец массива для освобождения
                tokens[position].type = TOKEN_END;
                tokens[position].text = NULL;
                free_tokens(tokens);
                
                return NULL;
            }

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

                    // если мы не в кавычках или после \ стоит " или \, то печатаем их
                    if (quote == '\0' || 
                        ptr[1] == '"' || ptr[1] == '\\') {
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
                    if (*ptr == ' ' || *ptr == '\t' || 
                        strchr("|&;()<>", *ptr) != NULL) {
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

void print_tokens(const Token *tokens) {
    for (size_t i = 0; tokens[i].type != TOKEN_END; i++) {
        if (tokens[i].type == TOKEN_WORD) {
            printf("WORD \"");
        } else {
            printf("OPERATOR \"");
        }

        for (const char *ptr = tokens[i].text; *ptr != '\0'; ptr++) {
            switch(*ptr) {
                case '\n':
                    printf("\\n");
                    break;
                case '\t':
                    printf("\\t");
                    break;
                case '\\':
                    printf("\\\\");
                    break;
                case '"':
                    printf("\\\"");
                    break;
                default:
                    putchar(*ptr);
                    break;
            }
        }
        
        printf("\"\n");
    }

    printf("END\n");
}