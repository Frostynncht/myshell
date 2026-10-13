#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include "runner.h"
#include "commands.h"

static void set_redirect(char **cmd, const Token *cmd_tokens, int *position) {
    TokenType type = cmd_tokens[*position].type;

    (*position)++; // переход от оператора к токену с именем файла

    // проверка на наличие имени файла после перенаправления
    if (cmd[*position] == NULL || cmd_tokens[*position].type != TOKEN_WORD) {
        fprintf(stderr, "mysh: expected file name after redirection\n");
        _exit(1);
    }

    int flags;
    int target;

    if (type == TOKEN_INPUT) {
        flags = O_RDONLY;
        target = STDIN_FILENO;
    } else if (type == TOKEN_OUTPUT) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        target = STDOUT_FILENO;
    } else {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        target = STDOUT_FILENO;
    }

    int fd = open(cmd[*position], flags, 0666);
    if (fd == -1) {
        perror("mysh: open");
        _exit(1);
    }

    if (dup2(fd, target) == -1) {
        perror("mysh: dup2");
        _exit(1);
    }

    // закрываем лишний fd(target открыт)
    if (fd != target && close(fd) == -1) {
        perror("mysh: close");
        _exit(1);
    }

}

// выполняет одну команду - отдельно или внутри конвейцра
static void run_simple_command(char **cmd, const Token *cmd_tokens) {
    char *cmd_args[128]; // массив аргументов для execvp
    int cmd_arg = 0; // счетчик аргументов

    // обработка перенаправления ввода/вывода
    for (int i = 0; cmd[i] != NULL; i++) {
        TokenType type = cmd_tokens[i].type;

        if (type == TOKEN_OUTPUT || type == TOKEN_INPUT || type == TOKEN_APPEND) {
            set_redirect(cmd, cmd_tokens, &i);
        } else {
            // если не символ перенаправления то добавляем в массив аргументов
            cmd_args[cmd_arg++] = cmd[i];
        }
    }
    // NULL в конец массива для execvp
    cmd_args[cmd_arg] = NULL;

    signal(SIGINT, SIG_DFL); // возвращаем обработку Ctrl+C в дочернем процессе
    
    if (cmd_arg > 0) {
        if (strcmp(cmd_args[0], "echo") == 0) {
            int result = run_echo(cmd_args);
            _exit(result);
        }

        execvp(cmd_args[0], cmd_args);
        perror("mysh");
        _exit(127);
    }
    _exit(0);
}

static int run_pipe(char **args, const Token *tokens, int pipe_count) {
    int command_count = pipe_count + 1;
    pid_t *pids = malloc(command_count * sizeof(pid_t));
    
    if (pids == NULL) {
        perror("mysh: malloc");
        return 1;
    }

    int prev_read = -1;
    int start = 0;
    int started = 0; // сколько детей получилось создать
    int failed = 0; // была ли ошибка запуска или ожидания

    // одна команда - один проход цикла
    for (int command = 0; command < command_count; command++) {
        int end = start;
        
        // читаем до | или конца массива
        while (args[end] != NULL && tokens[end].type != TOKEN_PIPE) {
            end++;
        }

        int have_next;
        // есть ли еще команда
        if (command + 1 < command_count) {
            have_next = 1;
        } else {
            have_next = 0;
        }

        // два конца нового канала
        int pipefd[2] = {-1, -1};

        // если есть еще команда
        if (have_next == 1) {
            args[end] = NULL; // отделяет команду от следующей
            
            // создаем канал от текущей до следующецй команде
            if (pipe(pipefd) == -1) {
                perror("mysh: pipe");
                failed = 1;
                break;
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("mysh: fork");
            // ребенка не создали - новый канал теперь не нужен
            if (have_next == 1) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            failed = 1;
            break;
        // настраиваем ввод и вывод в ребенке
        } else if (pid == 0) {
            // если есть предыдущая команда, то делаем ввод от предыдущей команды
            if (prev_read != -1 && dup2(prev_read, STDIN_FILENO) == -1) {
                perror("mysh: dup2");
                _exit(1);
            }         

            // если есть следующая команда, то делаем вывод в новый канал
            if (have_next == 1 && dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("mysh: dup2");
                _exit(1);
            }

            // после dup2 старый номер чтения ребенку больше не нужен
            if (prev_read != -1) {
                close(prev_read);
            }

            // закрываем неиспользуемые номера нового канала
            if (have_next == 1) {
                close(pipefd[0]);
                close(pipefd[1]);
            }

            // выполняет одну команду
            run_simple_command(&args[start], &tokens[start]);
            _exit(1);
        }

        // родитель записывает pid ребенка в массив(для waitpid)
        pids[started] = pid;
        started++;

        // родителю не нужен предыдущий канал чтения
        if (prev_read != -1) {
            close(prev_read);
        }

        // родитель закрывает новый канал для записи 
        if (have_next == 1) {
            close(pipefd[1]);
            prev_read = pipefd[0];
        } else { // нового канала больше нет
            prev_read = -1;
        }

        // переводим на первый токен следующей команды
        start = end + 1;
    }

    // если запуск 
    if (prev_read != -1) {
        // получилось ли закрыть предыдущий канал
        if (close(prev_read) == -1) {
            perror("mysh: close");
            failed = 1;
        }
    }

    // результат последней команды
    int last_status = 0;
    
    // ждем результатов детей которых уде запустили
    for (int i = 0; i < started; i++) {
        int status;
        pid_t result;

        // первый раз ждем этого ребенка
        result = waitpid(pids[i], &status, 0);
        // если ожидание прервал сигнал - повторяем вызов
        while (result == -1 && errno == EINTR) {
            result = waitpid(pids[i], &status, 0);
        }

        if (result == -1) {
            perror("mysh: waitpid");
            failed = 1;
        } else if (i == started - 1) { // сохраняем статус последней команды
            last_status = status;
        }
    }

    // массив ids больше не нужен
    free(pids);

    // проверка были ли ошибка
    if (failed == 1) {
        return 1;
    }

    // показывает как завершилась последняя команда
    if (WIFEXITED(last_status)) {
        return WEXITSTATUS(last_status);
    }

    return 1;
}

int runner(char **args, const Token *tokens) {
    if (args[0] == NULL) {
        return 0;
    }

    if (fflush(stdout) == EOF) {
        perror("mysh: runner");
        return 1;
    }

    int pipe_count = 0;
    // проверка на наличие |
    for (int i = 0; args[i] != NULL; i++) {
        // ищем только | 
        if (tokens[i].type != TOKEN_PIPE) {
            continue;
        }

        if (i == 0 || args[i + 1] == NULL || tokens[i + 1].type == TOKEN_PIPE || tokens[i - 1].type == TOKEN_PIPE) {
            fprintf(stderr, "mysh: expected command near |\n");
            return 1;
        }

        pipe_count++;
    }

    if (pipe_count > 0) {
        return run_pipe(args, tokens, pipe_count);
    }

    // если нет символа |
    pid_t pid = fork();
    if (pid == -1) {
        perror("mysh: fork");
        return 1;
    } else if (pid == 0) {
        run_simple_command(args, tokens);
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