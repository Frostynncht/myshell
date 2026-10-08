#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "commands.h"

int run_pwd(void) {
    char *dir = get_curr_dir();

    if (dir == NULL) {
        return 1;
    }

    printf("%s\n", dir);
    free(dir);
    return 0;
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
        if( i > first_arg) {
            printf(" ");
        }
        printf("%s", arg[i]);
    }

    if (print_newline == 1) {
        printf("\n");
    }

    // проверка вывелось ли все в stdout
    if (fflush(stdout) == EOF) {
        perror("mysh: echo");
        return 1;
    }

    return 0;
}