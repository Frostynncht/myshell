#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "lexer.h"

int main() {
    printf("myshell: shell started\n");

    char line[1024];

    while (1) {
        write(STDOUT_FILENO, "mysh$ ", 6);

        if (fgets( line, sizeof(line), stdin) == NULL) { // записывает в line ввод с клавиатуры(stdin) размером не более line(1023)
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        char **arg = split_line(line);

        if (arg[0] == NULL) {
            free_arg(arg);
            break;
        }

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
        } else if (pid == 0) {
            execvp(arg[0], arg);
            perror("mysh");
            _exit(127); // команда не найдена
        } else {
            int status;
            wait(&status);
        }
        free_arg(arg);
    }
    
    return 0;
}