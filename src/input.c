#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "input.h"
#include "commands.h"
#include "lexer.h"

char *read_line(int interactive) {
    size_t capacity = 64;
    size_t len = 0;
    char *line = malloc(capacity);

    if (line == NULL) {
        perror("mysh: malloc");
        return NULL;
    }

    int input_char; 

    // читаем команду полностью вместе с продолжением строки
    while((input_char = getchar()) != EOF) {
        if (input_char == '\n') {
            line[len] = '\0';
            LineState state = get_line_state(line);

            if (state == LINE_READY) {
                break;
            }

            if (state == LINE_SLASH) {
                // убираем завершающий "\"
                len--;
            }

            // вывод приглашения при продолжении в терминале
            if (interactive == 1) {
                if (printf("> ") < 0 || fflush(stdout) == EOF) {
                    perror("mysh: prompt");
                    free(line);
                    return NULL;
                }
            }

            if (state == LINE_SLASH) {
                // Enter тоже не записываем
                continue;
            }
        }

        // перевыделяем память под строку
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *new_line = realloc(line, capacity);

            if (new_line == NULL) {
                perror("mysh: realloc");
                free(line);
                return NULL;
            }

            line = new_line;
        }

        line[len] = (char)input_char;
        len++;
    }

    // проверяем произошла ли ошибка при чтении
    if (ferror(stdin)) {
        perror("mysh: input");
        free(line);
        return NULL;
    }

    // если ввод закончился до первого символа - строки нет
    if (input_char == EOF && len == 0) {
        free(line);
        return NULL;
    }

    line[len] = '\0';
    return line;
}

int print_prompt(void) {
    // при вводе из файла или пайпа приглашение не выводим
    if (isatty(STDIN_FILENO) == 0) {
        return 1;
    }

    char *dir = get_curr_dir();

    if (dir == NULL) {
        return 0;
    }

    // выводим текущую директорию и знак 
    if (printf("%s$ ", dir) < 0 || fflush(stdout) == EOF) {
        perror("mysh: prompt");
        free(dir);
        return 0;
    }

    free(dir);
    return 1;
}