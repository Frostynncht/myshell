#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    printf("myshell: shell started\n");

    char line[1024];

    while (1) {
        write(STDOUT_FILENO, "mysh$ ", 6);

        if (fgets( line, sizeof(line), stdin) == NULL) { // записывает в line ввод с клавиатуры размером не более line(1023)
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

        printf("Read: [%s]\n", line);
    }
    
    return 0;
}