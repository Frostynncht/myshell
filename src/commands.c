#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "commands.h"

// выводит текущий каталог, аргументы не поддерживаются
int run_pwd(char **arg) {
    if (arg[1] != NULL) {
        fprintf(stderr, "mysh: pwd: no arguments expected\n");
        return 2;
    }

    char *dir = get_curr_dir();
    if (dir == NULL) {
        return 1;
    }

    int result = 0;

    // проверка удалось ли записать путь 
    if (printf("%s\n", dir) < 0 || fflush(stdout) == EOF || ferror(stdout)) {
        perror("mysh: pwd");
        result = 1;
    }

    free(dir);
    return result;
}

char *get_curr_dir(void) {
    size_t capacity = 64;
    char *dir = malloc(capacity);

    if (dir == NULL) {
        perror("mysh: malloc");
        return NULL;
    }

    while (getcwd(dir, capacity) == NULL) {
        // если ошибка не из-за маленького буфера
        if (errno != ERANGE) {
            perror("mysh: getcwd");
            free(dir); 
            return NULL;
        }

        capacity *= 2;
        char *new_dir = realloc(dir, capacity);

        if (new_dir == NULL) {
            perror("mysh: realloc");
            free(dir);
            return NULL;
        }

        dir = new_dir;
    }

    return dir;
}

int run_cd(char **arg) {
    if (arg[1] != NULL &&arg[2] != NULL) {
        fprintf(stderr, "mysh: cd: too many arguments\n");
        return 1;
    }

    char *target_dir = arg[1];
    
    // если аргумент не указан, то переходим в домашнюю директорию
    if (target_dir == NULL) {
        target_dir = getenv("HOME");

        if (target_dir == NULL) {
            fprintf(stderr, "mysh: cd: HOME not set\n");
            return 1;
        }
    }
    // переходим в указанную директорию
    if (chdir(target_dir) != 0) {
        perror("mysh: cd");
        return 1;
    }
    
    return 0;
}

int run_echo(char **arg) {
    int first_arg = 1;
    int print_newline = 1;

    // если первый аргумент -n, то не печатаем перевод строки
    if (arg[1] != NULL && strcmp(arg[1], "-n") == 0) {
        first_arg = 2;
        print_newline = 0;
    }

    // печатаем все аргументы начиная с first_arg
    for (int i = first_arg; arg[i] != NULL; i++) {
        if( i > first_arg && printf(" ") < 0) {
            perror("mysh: echo");
            return 1;
        }
        if (printf("%s", arg[i]) < 0) {
            perror("mysh: echo");
            return 1;
        }
    }

    if (print_newline == 1 && printf("\n") < 0) {
        perror("mysh: echo");
        return 1;
    }

    // проверка вывелось ли все в stdout
    if (fflush(stdout) == EOF || ferror(stdout)) {
        perror("mysh: echo");
        return 1;
    }

    return 0;
}

int run_help(void) {
    printf("cd [dir] - change current directory\n");
    printf("pwd - print current directory\n");
    printf("echo [-n] [args] - print arguments\n");
    printf("help - show this help\n");
    printf("exit [code] - close the shell\n");

    if (fflush(stdout) == EOF || ferror(stdout)) {
        perror("mysh: help");
        return 1;
    }

    return 0;
}

// получает код exit, но не завершает процесс 
int get_exit_code(char **arg, int last_status, int *exit_status) {
    *exit_status = last_status;

    if (arg[1] == NULL) {
        return 1;
    }

    if (arg[2] != NULL) {
        fprintf(stderr, "mysh: exit: too many arguments\n");
        return 0;
    }

    const char *number_text = arg[1];
    int i = 0;
    int neg = 0;
    int valid = 1;
    int number = 0;

    // пропускаем +, запоминаем -
    if (number_text[i] == '+' || number_text[i] == '-') {
        if (number_text[i] == '-') {
            neg = 1;
        }
        i++;
    }

    // после знака должна быть хотя бы одна цифра
    if (number_text[i] == '\0') {
        valid = 0;
    }

    while (number_text[i] != '\0') {
        if (number_text[i] < '0' || number_text[i] > '9') {
            valid = 0;
            break;
        }

        // собираем число в пределах от 0 до 255
        number = (number * 10 + (number_text[i] - '0')) % 256;
        i++;   
    }

    if (valid == 0) {
        fprintf(stderr, "mysh: exit: expected number argument\n");
        return 0;
    }

    if (neg == 1) {
        number = (256 - number) % 256;
    }

    *exit_status = number;
    return 1;
}