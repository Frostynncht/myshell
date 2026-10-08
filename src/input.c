#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "input.h"
#include "commands.h"

char *read_line(void) {
    size_t capacity = 64;
    size_t len = 0;
    char *line = malloc(capacity);

    if (line == NULL) {
        perror("mysh: malloc");
        return NULL;
    }

    int input_char; 

    // читаем символы до Enter или конца ввода
    while((input_char = getchar()) != '\n' && input_char != EOF) {
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