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

// выполняет простую команду в дочерном процессе
static void run_simple_command(const Node *node, int previous_status) {
    signal(SIGINT, SIG_DFL);

    // перенаправления применяем после настройки каналов
    const Redirection *redirect = node->redirects;
    while(redirect != NULL) {
        if (set_redirect(redirect->type, redirect->file_name) == 0) {
            _exit(1);
        }

        redirect = redirect->next;
    }

    int result;

    // встроенные команды внутри конвейера выполняются в ребенке
    if (strcmp(node->arg[0], "exit") == 0) {
        if (get_exit_code(node->arg, previous_status, &result) == 0) {
            result = 2;
        }
    } else if (strcmp(node->arg[0], "cd") == 0) {
        result = run_cd(node->arg);
    } else if (strcmp(node->arg[0], "pwd") == 0) {
        result = run_pwd();
    } else if (strcmp(node->arg[0], "echo") == 0) {
        result = run_echo(node->arg);
    } else if (strcmp(node->arg[0], "help") == 0) {
        result = run_help();
    } else {
        // внешняя программа заменяет программу ребенка
        execvp(node->arg[0], node->arg);
        
        // это только при ошибке запуска
        result = 126;
        if (errno == ENOENT) {
            result = 127;
        }

        perror("mysh");
        _exit(result);
    }

    // перед _exit выводим оставшуюся часть встроенной команды
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        result = 1;
    }

    _exit(result);
}

// запускает все команды конвейера по данным из дерева
int run_pipe_node(const Node *tree, int previous_status) {
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        return 1;
    }

    // считаем команды, идем по левой ветке конвейера
    int command_count = 1;
    const Node *node = tree;

    while(node->type == NODE_PIPE) {
        command_count++;
        node = node->left;
    }

    // массив указателей на команды в порядке запуска
    const Node **commands = malloc(command_count * sizeof(*commands));
    if (commands == NULL) {
        perror("mysh: malloc");
        return 1;
    }

    // заполняем правые команды с конца справа налево
    node = tree;
    int position = command_count - 1;
    
    while(node->type == NODE_PIPE) {
        commands[position] = node->right;
        position--;
        node = node->left;
    }

    commands[0] = node;

    pid_t *pids = malloc(command_count * sizeof(*pids));
    if (pids == NULL) {
        perror("mysh: malloc");
        free(commands);
        return 1;
    }

    int prev_read = -1;
    int started = 0; // сколько детей успешно создалось
    int failed = 0; // была ли ошибка запуска или ожидания

    // одна команда - один проход цикла
    for (int command = 0; command < command_count; command++) {
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
            run_simple_command(commands[command], previous_status);
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
    free(commands);

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

// выполняет встроенные команды с перенаправлениями
int run_my_command_node(const Node *node, int previous_status, int *end_shell) {
    // по умолчанию после команды продолжаем работу оболочки
    *end_shell = 0;

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
        } else if (strcmp(node->arg[0], "exit") == 0) {
            // получаем код exit только после успешных перенаправлений
            if (get_exit_code(node->arg, previous_status, &result) == 1) {
                *end_shell = 1;
            } else {
                result = 2;
            }
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
        run_simple_command(node, 0);
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