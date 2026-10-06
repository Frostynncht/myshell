#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include "runner.h"

// чтобы не дублировать код в случае пайпа
static void run_simple_command(char **cmd) {
    char *cmd_args[128]; // массив аргументов для execvp
    int cmd_arg = 0; // счетчик аргументов

    for (int i = 0; cmd[i] != NULL; i++) {
        // обработка >
        if (strcmp(cmd[i], ">") == 0) {
            // пропуск >
            i++;
            // проверка на наличие имени после символа >
            if (cmd[i] == NULL) {
                write(STDERR_FILENO, "mysh: missing file name or redirection\n", 40);
                _exit(1);
            }

            // переписываем файл если он существует или создает новый
            int fd = open(cmd[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                perror("mysh: open");
                _exit(1);
            }

            // перенаправление вывода в файл
            dup2(fd, STDOUT_FILENO);
            close(fd);

        // обработка >>
        }else if (strcmp(cmd[i], ">>") == 0) {
            i++;
            if (cmd[i] == NULL) {
                write(STDERR_FILENO, "mysh: missing file name or redirection\n", 40);
                _exit(1);
            }

            // записываем в конец файла если он существует или создает новый
            int fd = open(cmd[i], O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd == -1) {
                perror("mysh: open");
                _exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);

        // обработка <
        } else if (strcmp(cmd[i], "<") == 0) {
            i++;
            if (cmd[i] == NULL) {
                write(STDERR_FILENO, "mysh: missing file name or redirection\n", 40);
                _exit(1);
            }

            // открываем файл только для чтения
            int fd = open(cmd[i], O_RDONLY);
            if (fd == -1) {
                perror("mysh: open");
                _exit(1);
            }

            // перенаправление ввода из файла
            dup2(fd, STDIN_FILENO);
            close(fd);

        } else {
            // если не символ перенаправления то добавляем в массив аргументов
            cmd_args[cmd_arg++] = cmd[i];
        }
    }
    // NULL в конец массива для execvp
    cmd_args[cmd_arg] = NULL;

    if (cmd_arg > 0) {
        signal(SIGINT, SIG_DFL); // возвращаем обработку Ctrl+C в дочернем процессе
        execvp(cmd_args[0], cmd_args);
        perror("mysh");
        _exit(127);
    }
    _exit(0);
}

int runner(char **args) {
    if (args[0] == NULL) {
        return 0;
    }

    int pipe_pos = -1;
    // проверка на наличие |
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            // если есть символ |, то запоминаем его позицию
            pipe_pos = i;
            break;
        }
    }

    if (pipe_pos != -1) {
        // разбиваем массив на две части(если есть символ |)
        args[pipe_pos] = NULL;
        char **cmd1 = args; // слева от |
        char **cmd2 = &args[pipe_pos + 1];// справа от |

        // создаем пайп
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            perror("mysh: pipe");
            return 1;
        }

        // создаем первый процесс для команды слева от |
        pid_t pid1 = fork();
        if (pid1 == -1) {
            perror("mysh: fork");
            close(pipefd[0]);
            close(pipefd[1]);
            return 1;

        } else if (pid1 == 0) {
            close(pipefd[0]); // закрываем конец для чтения в дочернем процессе
            dup2(pipefd[1], STDOUT_FILENO); // перенаправляем stdout в конец для записи пайпа
            close(pipefd[1]);
            run_simple_command(cmd1);
        }
        
        // создаем второй процесс для команды справа от |
        pid_t pid2 = fork();
        if (pid2 == -1) {
            perror("mysh: fork");
            close(pipefd[0]);
            close(pipefd[1]);
            return 1;

        } else if (pid2 == 0) {
            close(pipefd[1]); // закрываем конец для записи в дочернем процессе
            dup2(pipefd[0], STDIN_FILENO); // перенаправляем stdin в конец для чтения пайпа
            close(pipefd[0]);
            run_simple_command(cmd2);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        int status;
        waitpid(pid1, NULL, 0); // NULL, так как нам не нужен статус первого процесса
        waitpid(pid2, &status, 0); // ждем завершения второго процесса и получаем его статус

        // возвращаем код завершения второго процесса
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 0;
        }
    }

    // если нет символа |
    pid_t pid = fork();
    if (pid == -1) {
        perror("mysh: fork");
        return 1;
    } else if (pid == 0) {
        run_simple_command(args);
    } else {
        int status;
        waitpid(pid, &status, 0);
        
        // возвращаем код завершения дочернего процесс
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 0;
        }
    }

    return 0;
}