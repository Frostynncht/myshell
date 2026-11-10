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

// открывает файл и перенаправляет ввод и вывод
static int set_redirect(TokenType type, const char *file_name) {
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

    int fd = open(file_name, flags, 0666);
    if (fd == -1) {
        perror("mysh: open");
        return 0;
    }

    if (dup2(fd, target) == -1) {
        perror("mysh: dup2");
        close(fd);
        return 0;
    }

    // после dup2 номер fd больше не нужен
    if (fd != target && close(fd) == -1) {
        perror("mysh: close");
        return 0;
    }

    return 1;
}

// выполняет одну команду - отдельно или внутри конвейцра
static void run_simple_command(char **cmd, const Token *cmd_tokens) {
    // cчитаем токены для выделения памяти под массив аргументов
    int token_count = 0;
    while(cmd[token_count] != NULL) {
        token_count++;
    }

    char **cmd_args = malloc(((size_t)token_count + 1) * sizeof(*cmd_args));
    if (cmd_args == NULL) {
        perror("mysh: malloc");
        _exit(1);
    }

    int cmd_arg = 0;

    // обработка перенаправления ввода/вывода
    for (int i = 0; cmd[i] != NULL; i++) {
        TokenType type = cmd_tokens[i].type;

        if (type == TOKEN_OUTPUT || 
            type == TOKEN_INPUT || 
            type == TOKEN_APPEND) {
            i++;

            if (cmd[i] == NULL || cmd_tokens[i].type != TOKEN_WORD) {
                fprintf(stderr, "mysh: expected file name after redirection\n");
                free(cmd_args);
                _exit(1);
            }

            if (set_redirect(type, cmd[i]) == 0) {
                free(cmd_args);
                _exit(1);
            }
        } else {
            // если не символ перенаправления то добавляем в массив аргументов
            cmd_args[cmd_arg++] = cmd[i];
        }
    }
    // NULL в конец массива для execvp
    cmd_args[cmd_arg] = NULL;

    signal(SIGINT, SIG_DFL); // возвращаем обработку Ctrl+C в дочернем процессе
    
    if (cmd_arg > 0) {
        if (strcmp(cmd_args[0], "exit") == 0) {
            free(cmd_args);
            _exit(0);
        }

        if (strcmp(cmd_args[0], "cd") == 0) {
            int result = run_cd(cmd_args);
            free(cmd_args);
            _exit(result);
        }

        if (strcmp(cmd_args[0], "pwd") == 0) {
            int result = run_pwd();

            if (fflush(stdout) == EOF) {
                perror("mysh: pwd");
                result = 1;
            }

            free(cmd_args);
            _exit(result);
        }

        if (strcmp(cmd_args[0], "echo") == 0) {
            int result = run_echo(cmd_args);
            free(cmd_args);
            _exit(result);
        }

        if (strcmp(cmd_args[0], "help") == 0) {
            int result = run_help();
            free(cmd_args);
            _exit(result);
        }

        // при успехе execvp заменяет программу ребенка - дальше обработка только ошибки
        execvp(cmd_args[0], cmd_args);
        
        int exit_status = 126;
        if (errno == ENOENT) {
            exit_status = 127;
        }
        
        perror("mysh");
        free(cmd_args);
        _exit(exit_status);
    }

    free(cmd_args);
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

    // если последняя команда завершилась по сигналу
    if (WIFSIGNALED(last_status)) {
        return 128 + WTERMSIG(last_status);
    }

    return 1;
}

// выполняет встроенные команды cd, pwd, echo, help из узла дерева команд
int run_my_command_node(const Node *node) {
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        return 1;
    }

    int saved_input = dup(STDIN_FILENO);
    if (saved_input == -1) {
        perror("mysh: dup");
        return 1;
    }

    // сохраняем stdout чтобы вернуть его после перенаправления
    int saved_output = dup(STDOUT_FILENO);
    if (saved_output == -1) {
        perror("mysh: dup");

        if (close(saved_input) == -1) {
            perror("mysh: close");
        }

        return 1;
    }

    int result = 1;
    int redirects_ok = 1;

    const Redirection *redirect = node->redirects;
    while(redirect != NULL) {
        if (set_redirect(redirect->type, redirect->file_name) == 0) {
            redirects_ok = 0;
            break;
        }

        redirect = redirect->next;
    }

    // выполняем нужную встроенную команду
    if (redirects_ok == 1) {
        if (strcmp(node->arg[0], "cd") == 0) {
            result = run_cd(node->arg);
        } else if (strcmp(node->arg[0], "pwd") == 0) {
            result = run_pwd();
        } else if (strcmp(node->arg[0], "echo") == 0) {
            result = run_echo(node->arg);
        } else if (strcmp(node->arg[0], "help") == 0) {
            result = run_help();
        } 
    }

    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        result = 1;
    }

    // возвращаем оболочке прежние ввод и вывод
    if (dup2(saved_input, STDIN_FILENO) == -1) {
        perror("mysh: dup2");
        result = 1;
    }

    if (dup2(saved_output, STDOUT_FILENO) == -1) {
        perror("mysh: dup2");
        result = 1;
    }

    // закрываем дополнительные дескрипторы после восстановления
    if (close(saved_input) == -1) {
        perror("mysh: close");
        result = 1;
    }

    if (close(saved_output) == -1) {
        perror("mysh: close");
        result = 1;
    }

    return result;
}

// запускает отдельную программу по данным из узла дерева
int run_simple_node(const Node *node) {
    if (fflush(stdout) == EOF) {
        perror("mysh: runner");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("mysh: fork");
        return 1;
    } else if (pid == 0) {
        // возвращаем реагирование на ctrl+c
        signal(SIGINT, SIG_DFL);

        // настраиваем перенаправления по порядку из списка
        const Redirection *redirect = node->redirects;
        while(redirect != NULL) {
            if (set_redirect(redirect->type, redirect->file_name) == 0) {
                _exit(1);
            }

            redirect = redirect->next;
        }

        // запускаем программу с аргументами от парсера
        execvp(node->arg[0], node->arg);
        // ниже при ошибке execvp
        
        int exit_status = 126;
        if (errno == ENOENT) {
            exit_status = 127;
        }

        perror("mysh");
        _exit(exit_status);
    } else {
        int status;
        pid_t result = waitpid(pid, &status, 0);

        // если сигнал прервал ожидаение - ждем еще раз ребенка
        while (result == -1 && errno == EINTR) {
            result = waitpid(pid, &status, 0);
        }

        if (result == -1) {
            perror("mysh: waitpid");
            return 1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }

        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }

        return 1;
    }

    return 0;
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

        if (i == 0 || args[i + 1] == NULL || 
            tokens[i + 1].type == TOKEN_PIPE || 
            tokens[i - 1].type == TOKEN_PIPE) {
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
        pid_t result = waitpid(pid, &status, 0);

        while (result == -1 && errno == EINTR) {
            result = waitpid(pid, &status, 0);
        }

        if (result == -1) {
            perror("mysh: waitpid");
            return 1;
        }
        
        // возвращаем код завершения дочернего процесс
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } 

        // если дочерний процесс завершился по сигналу
        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }

        return 1;
    }

    return 0;
}