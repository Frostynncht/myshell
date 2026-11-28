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
#include "jobs.h"

// закрывает дескриптор и сообщает об ошибке
static int close_fd(int fd) {
    if (close(fd) == -1) {
        perror("mysh: close");
        return 0;
    }
    
    return 1;
}

// возвращает обычную обработку сигналов в ребенке
static int reset_child_signal(void) {
    if (signal(SIGINT, SIG_DFL) == SIG_ERR ||
    signal(SIGQUIT, SIG_DFL) == SIG_ERR ||
    signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
    signal(SIGTSTP, SIG_DFL) == SIG_ERR ||
    signal(SIGTTIN, SIG_DFL) == SIG_ERR ||
    signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
        perror("mysh: signal");
        return 0;
    }

    return 1;
}

// повторяем dup2 если вызов прервал сигнал
static int copy_fd(int fd, int target) {
    int result = dup2(fd, target);

    while (result == -1 && errno == EINTR) {
        result = dup2(fd, target);
    }
    
    return result;
}

// сохраняем дескриптор при прерывании сигналом
static int save_fd(int fd) {
    int result = dup(fd);

    while (result == -1 && errno == EINTR) {
        result = dup(fd);
    }

    return result;
}

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

    // если открытие файла прервал сигнал - повторяем 
    while (fd == -1 && errno == EINTR) {
        fd = open(file_name, flags, 0666);
    }

    if (fd == -1) {
        fprintf(stderr, "mysh: %s: %s\n", file_name, strerror(errno));
        return 0;
    }

    if (copy_fd(fd, target) == -1) {
        perror("mysh: dup2");
        close_fd(fd);
        return 0;
    }

    // после dup2 номер fd больше не нужен
    if (fd != target && close_fd(fd) == 0) {
        return 0;
    }

    return 1;
}

// запускает пронрамме а при неудаче возвращает номер ошибки
static int exec_program(char **arg) {
    if (arg[0][0] == '\0') {
        return ENOENT;
    }

    if (strchr(arg[0], '/') != NULL) {
        execv(arg[0], arg);
        return errno;
    }

    // получаем каталоги поиска если нету то PATH используем стандартные
    const char *path = getenv("PATH");
    if (path == NULL) {
        path = "/bin:/usr/bin";
    }

    int permission_error = 0;
    size_t name_len = strlen(arg[0]);

    while (1) {
        size_t dir_len = 0;
        // считаем символы текущего каталога до : или конца PATH
        while (path[dir_len] != ':' && path[dir_len] != '\0') {
            dir_len++;
        }

        // место для каталога, /, имени программы и нуля в конце
        char *file_name = malloc(dir_len + name_len + 2);
        if (file_name == NULL) {
            return ENOMEM;
        }

        size_t position = 0;

        if (dir_len != 0) {
            // копируем только текущий каталог, без остальной части PATH
            memcpy(file_name, path, dir_len);
            position = dir_len;
            file_name[position] = '/';
            position++;
        }

        // дописываем имя программы с нулем в конце
        strcpy(file_name + position, arg[0]);

        execv(file_name, arg);

        int saved_errno = errno;
        free(file_name);

        // продолжаем поиск только если нет файла или запрет доступа
        if (saved_errno == EACCES) {
            permission_error = 1;
        } else if (saved_errno != ENOENT && saved_errno != ENOTDIR) {
            return saved_errno;
        }

        // если текущий каталог последний, заканчиваем поиск
        if (path[dir_len] == '\0') {
            break;
        }

        path += dir_len + 1;
    }

    if (permission_error == 1) {
        return EACCES;
    }

    return ENOENT;
}

// выполняет простую команду в дочерном процессе
static void run_simple_command(const Node *node, int previous_status) {
    if (reset_child_signal() == 0) {
        _exit(1);
    }

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
        result = run_pwd(node->arg);
    } else if (strcmp(node->arg[0], "echo") == 0) {
        result = run_echo(node->arg);
    } else if (strcmp(node->arg[0], "help") == 0) {
        result = run_help();
    } else {
        // при успехе программа заменяет ребенка а при ошибке получаем ее номер
        int saved_errno = exec_program(node->arg);
        
        result = 126;
        if (saved_errno == ENOENT) {
            result = 127;
        }

        fprintf(stderr, "mysh: %s: %s\n", node->arg[0], strerror(saved_errno));
        _exit(result);
    }

    // перед _exit выводим оставшуюся часть встроенной команды
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        result = 1;
    }

    _exit(result);
}

static void run_subshell_command(const Node *node, int previous_status) {
    if (reset_child_signal() == 0) {
        _exit(1);
    }

    const Redirection *redirect = node->redirects;
    while(redirect != NULL) {
        if (set_redirect(redirect->type, redirect->file_name) == 0) {
            _exit(1);
        }

        redirect = redirect->next;
    }

    int end_shell = 0;
    // внутри подоболочки управление терминалом отключено
    int result = run_tree(node->left, previous_status, &end_shell, 0);

    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        result = 1;
    }

    _exit(result);
}

// запускает все команды конвейера по данным из дерева
int run_pipe_node(const Node *tree, int previous_status, int interactive) {
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
    pid_t pgid = 0; // общая группа процессов для всего конвейера
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
                close_fd(pipefd[0]);
                close_fd(pipefd[1]);
            }
            failed = 1;
            break;
        // настраиваем ввод и вывод в ребенке
        } else if (pid == 0) {
            // в интерактивном режиме переводим процесс в группу конвейера
            if (interactive == 1 && setpgid(0, pgid) == -1) {
                perror("mysh: setpgid");
                _exit(1);
            }
            // если есть предыдущая команда, то делаем ввод от предыдущей команды
            if (prev_read != -1 && copy_fd(prev_read, STDIN_FILENO) == -1) {
                perror("mysh: dup2");
                _exit(1);
            }         

            // если есть следующая команда, то делаем вывод в новый канал
            if (have_next == 1 && copy_fd(pipefd[1], STDOUT_FILENO) == -1) {
                perror("mysh: dup2");
                _exit(1);
            }

            // после dup2 старый номер чтения ребенку больше не нужен
            if (prev_read != -1) {
                if (close_fd(prev_read) == 0) {
                    _exit(1);
                }
            }

            // закрываем неиспользуемые номера нового канала
            if (have_next == 1) {
                if (close_fd(pipefd[0]) == 0 || close_fd(pipefd[1]) == 0) {
                    _exit(1);
                }
            }

            // выполняет одну команду
            if (commands[command]->type == NODE_SUBSHELL) {
                run_subshell_command(commands[command], previous_status);
            } else {
                run_simple_command(commands[command], previous_status);
            }

            _exit(1);
        }

        // родитель записывает pid ребенка в массив(для waitpid)
        pids[started] = pid;
        started++;

        if (interactive == 1) {
            // первый процесс задает номер группы всего конвейера
            if (pgid == 0) {
                pgid = pid;
            }
            if (setpgid(pid, pgid) == -1 && errno != EACCES && errno != ESRCH) {
                perror("mysh: setpgid");
            }
            if (command == 0 && tcsetpgrp(STDIN_FILENO, pgid) == -1) {
                perror("mysh: tcsetpgrp");
            }
        }

        // родителю не нужен предыдущий канал чтения
        if (prev_read != -1) {
            if (close_fd(prev_read) == 0) {
                failed = 1;
            }
        }

        // родитель закрывает новый канал для записи 
        if (have_next == 1) {
            if (close_fd(pipefd[1]) == 0) {
                failed = 1;
            }
            prev_read = pipefd[0];
        } else { // нового канала больше нет
            prev_read = -1;
        }
    }

    // закрываем оставшийся конец чтения после цикла
    if (prev_read != -1) {
        // получилось ли закрыть предыдущий канал
        if (close(prev_read) == -1) {
            perror("mysh: close");
            failed = 1;
        }
    }

    int last_status = 0; // результат последней команды
    int stopped = 0;
    int stop_sig = 0;
    int options = 0;
    if (interactive == 1) {
        options = WUNTRACED;
    }
    
    // ждем результатов детей которых уде запустили
    for (int i = 0; i < started; i++) {
        int status;
        pid_t result;

        // первый раз ждем этого ребенка
        result = waitpid(pids[i], &status, options);
        // если ожидание прервал сигнал - повторяем вызов
        while (result == -1 && errno == EINTR) {
            result = waitpid(pids[i], &status, options);
        }

        if (result == -1) {
            perror("mysh: waitpid");
            failed = 1;
        } else if (WIFSTOPPED(status)) {
            stopped = 1;
            stop_sig = WSTOPSIG(status);
        } else if (i == started - 1) { // сохраняем статус последней команды
            last_status = status;
        }
    }

    if (interactive == 1 && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("mysh: tcsetpgrp");
    }

    if (failed == 0 && stopped == 1) {
        int job_id = add_job(pgid, pids, started, JOB_STOPPED, "pipe");
        
        if (job_id != -1) {
            printf("\n[%d]+  Stopped              pipe\n", job_id);
            fflush(stdout);
        }

        free(pids);
        free(commands);

        return 128 + stop_sig;
    }

    // массив pids больше не нужен
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

    int saved_input = save_fd(STDIN_FILENO);
    if (saved_input == -1) {
        perror("mysh: dup");
        return 1;
    }

    // сохраняем stdout чтобы вернуть его после перенаправления
    int saved_output = save_fd(STDOUT_FILENO);
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
            result = run_pwd(node->arg);
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
    if (copy_fd(saved_input, STDIN_FILENO) == -1) {
        perror("mysh: dup2");
        result = 1;
    }

    if (copy_fd(saved_output, STDOUT_FILENO) == -1) {
        perror("mysh: dup2");
        result = 1;
    }

    // сбрасываем возможный флаг ошибки после возврата stdout
    clearerr(stdout);

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

// ждет завершения или остановки дочернего процесса и возвращает код его выхода
static int wait_child_status(pid_t pid, int interactive, const char *command) {
    int options = 0;
    if (interactive == 1) {
        // WUNTRACED ловит остановку по ctrl+z
        options = WUNTRACED;
    }
    
    int status;
    pid_t result = waitpid(pid, &status, options);

    // если сигнал прервал ожидание то ждем еще раз
    while (result == -1 && errno == EINTR) {
        result = waitpid(pid, &status, options);
    }

    // возвращаемс терминал оболочке после работы команды
    if (interactive == 1 && tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("mysh: tcsetpgrp");
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

    if (WIFSTOPPED(status)) {
        int job_id = add_job(pid, &pid, 1, JOB_STOPPED, command);
        if (job_id != -1) {
            printf("\n[%d]+ Stopped              %s\n", job_id, command);
            fflush(stdout);
        }
        return 128 + WSTOPSIG(status);
    }
    
    return 1;
}

// запускает отдельную программу по данным из узла дерева
int run_simple_node(const Node *node, int interactive) {
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("mysh: fork");
        return 1;
    } else if (pid == 0) {
        if (interactive == 1 && setpgid(0,0) == -1) {
            perror("mysh: setpgid");
            _exit(1);
        }

        run_simple_command(node, 0);
    }

    if (interactive == 1) {
        if (setpgid(pid, pid) == -1 && errno != EACCES && errno != ESRCH) {
            perror("mysh: setpgid");
        }
        if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
            perror("mysh: tcsetpgrp");
        }
    }

    return wait_child_status(pid, interactive, node->arg[0]);
}

// создает отдельный процесс для команды в скобках
static int run_subshell_node(const Node *node, int previous_result, int interactive) {
    if (fflush(stdout) == EOF) {
        perror("mysh: fflush");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("mysh: fork");
        return 1;
    } else if (pid == 0) {
        // переводим подоболочку в отдельную группу процессов
        if (interactive == 1 && setpgid(0, 0) == -1) {
            perror("mysh: setpgid");
            _exit(1);
        }

        run_subshell_command(node, previous_result);
    }

    if (interactive == 1) {
        // группу ставим и в родителе а терминал отдаем подоболочке
        if (setpgid(pid, pid) == -1 && errno != EACCES && errno != ESRCH) {
            perror("mysh: setpgid");
        }
        if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
            perror("mysh: tcsetpgrp");
        }
    }
     
    return wait_child_status(pid, interactive, "(subshell)");
}

// выполняет узел дерева и возвращает код последней команды
int run_tree(const Node *node, int previous_result, int *end_shell, int interactive) {
    if (node == NULL || *end_shell == 1) {
        return previous_result;
    }
    
    switch (node->type) {
        case NODE_SIMPLE:
            if (strcmp(node->arg[0], "cd") == 0 ||
            strcmp(node->arg[0], "pwd") == 0 ||
            strcmp(node->arg[0], "echo") == 0 ||
            strcmp(node->arg[0], "help") == 0 ||
            strcmp(node->arg[0], "exit") == 0) {
                return run_my_command_node(node, previous_result, end_shell);
            }

            return run_simple_node(node, interactive);

        case NODE_SUBSHELL:
            return run_subshell_node(node, previous_result, interactive);
        
        case NODE_PIPE:
            return run_pipe_node(node, previous_result, interactive);
        
        case NODE_AND:
        case NODE_OR:
        case NODE_SEQUENCE: {
            // сначал левую часть выполняем
            int status = run_tree(node->left, previous_result, end_shell, interactive);
            
            // после exit остальные команды не запускаем
            if (*end_shell == 1) {
                return status;
            }

            // для && правая часть только после успеха
            if (node->type == NODE_AND && status != 0) {
                return status;
            }
            
            // для || правая часть только после ошибки
            if (node->type == NODE_OR && status == 0) {
                return status;
            }

            // для ; правая часть независимо от левой
            return run_tree(node->right, status, end_shell, interactive);
        }

        default:
            fprintf(stderr, "mysh: not ready\n");
            return 2;
    }
}