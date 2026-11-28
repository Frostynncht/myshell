#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include "lexer.h"
#include "runner.h"
#include "commands.h"
#include "input.h"
#include "parser.h"
#include "jobs.h"

// проверяет умеет ли runner выполнить все дерево 
static int is_not_working(const Node *node) {
    if (node == NULL) {
        return 0;
    }

    if (node->type == NODE_BACKGROUND) {
        return 1;
    }

    // проверяем левые и правые ветви
    if (is_not_working(node->left) == 1) {
        return 1;
    }

    if (is_not_working(node->right) == 1) {
        return 1;
    }

    return 0;
}

// настраивает группу процессов и сигнали для интерактивной оболочки
static int init_terminal(void) {
    // оболочка ингорирует сигналы терминала
    if (signal(SIGINT, SIG_IGN) == SIG_ERR ||
    signal(SIGQUIT, SIG_IGN) == SIG_ERR ||
    signal(SIGTSTP, SIG_IGN) == SIG_ERR ||
    signal(SIGTTIN, SIG_IGN) == SIG_ERR ||
    signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("mysh: signal");
        return 0;
    }

    // переводим оболочку в свою группу процессов
    pid_t shell_pgid = getpid();
    if (getpgrp() != shell_pgid && setpgid(shell_pgid, shell_pgid) == -1) {
        perror("mysh: setpgid");
        return 0;
    }

    // переводим группу оболочки на передний план терминала
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("mysh: tcsetpgrp");
        return 0;
    }

    return 1;
}

// argc — количество аргументов запуска вместе с именем программы
// argv — строки с аргументами запуска
int main(int argc, char **argv) {
    int dump_tokens = 0;
    int dump_ast = 0;
    const char *command_text = NULL;

    // выбираем режим по аргументам запуска
    if (argc == 2 && strcmp(argv[1], "--dump-ast") == 0) {
        dump_ast = 1;
    } else if (argc == 2 && strcmp(argv[1], "--dump-tokens") == 0) {
        dump_tokens = 1;
    } else if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        command_text = argv[2];
    } else if (argc != 1) {
        fprintf(stderr, "mysh: usage: myshell '-c command | --dump-ast | --dump-tokens'\n");
        return 2;
    }

    // запоминаем код завершения помледней команды
    int last_status = 0;

    //запоминаем вводится ли команда с терминала
    int interactive = isatty(STDIN_FILENO);

    // в проверочных режимах и при -c приглашение не выводим
    if (dump_ast == 1 || dump_tokens == 1 || command_text != NULL) {
        interactive = 0;
    }

    if (interactive == 1) {
        if (init_terminal() == 0) {
            return 1;
        } 
    } else if (signal(SIGINT, SIG_IGN) == SIG_ERR ||
        signal(SIGQUIT, SIG_IGN) == SIG_ERR) {
        perror("mysh: signal");
        return 1;
    }

    init_jobs();

    while (1) {

        // если не удалось вывести приглашение - завершаем
        if (interactive == 1 && print_prompt() == 0) {
            return 1;
        }
        
        int input_error;
        char *line;

        if (command_text != NULL) {
            line = read_command(&command_text, &input_error);
        } else {
            line = read_line(interactive, &input_error);
        }

        // после Ctrl+D в терминале переходим на новую строку
        if (line == NULL) {
            if (input_error == 1) {
                return 1;
            }
            if (interactive == 1) {
                while(write(STDOUT_FILENO, "\n", 1) == -1) {
                    if (errno == EINTR) {
                        continue;
                    }

                    perror("mysh: write");
                    return 1;
                }
            }
            break;
        }

        Token *tokens = split_line(line);
        free(line); // больше не нужна(лексер делает копии)

        // проверка завершения лексера
        if (tokens == NULL) {
            last_status = 2;

            if (interactive == 0) {
                return last_status;
            }

            continue;
        }

        if (dump_tokens == 1) {
            print_tokens(tokens);
            free_tokens(tokens);
            return 0;
        }

        // пустая строка или комментарий не содержит команды
        if (tokens[0].type == TOKEN_END) {
            free_tokens(tokens);

            if (dump_ast == 1) {
                return 0;
            }
            
            continue;
        }

        // проверяем команду и строим дерево
        Node *tree = parse_tokens(tokens);
        if (tree == NULL) {
            last_status = 2;
            free_tokens(tokens);

            if (interactive == 0) {
                return last_status;
            }

            continue;
        }

        // показываем результат парсера и завершаемся до исполнения команд
        if (dump_ast == 1) {
            print_tree(tree, 0);
            free_tree(tree);
            free_tokens(tokens);
            return 0;
        }

        // не запускаем дерево с не готовыми операциями
        if (is_not_working(tree) == 1) {
            fprintf(stderr, "mysh: not ready\n");
            free_tree(tree);
            free_tokens(tokens);
            last_status = 2;

            if (interactive == 0) {
                return last_status;
            }

            continue;
        }
        
        // выполняем дерево
        int end_shell = 0;
        last_status = run_tree(tree, last_status, &end_shell, interactive);

        free_tree(tree);
        free_tokens(tokens);

        if (end_shell == 1) {
            free_jobs();
            return last_status;
        }
    }

    free_jobs();
    return last_status;
}