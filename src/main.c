#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "lexer.h"
#include "runner.h"
#include "commands.h"
#include "input.h"
#include "parser.h"

// проверяет умеет ли runner выполнить все дерево 
static int is_not_working(const Node *node) {
    if (node == NULL) {
        return 0;
    }

    if (node->type != NODE_SIMPLE && node->type != NODE_PIPE) {
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

// argc — количество аргументов запуска вместе с именем программы
// argv — строки с аргументами запуска
int main(int argc, char **argv) {
    int dump_tokens = 0;
    int dump_ast = 0;

    // выбираем режим по аргументам запуска
    if (argc == 2 && strcmp(argv[1], "--dump-ast") == 0) {
        dump_ast = 1;
    } else if (argc == 2 && strcmp(argv[1], "--dump-tokens") == 0) {
        dump_tokens = 1;
    } else if (argc != 1) {
        fprintf(stderr, "mysh: expected no argument, --dump-ast or --dump-tokens\n");
        return 2;
    }

    // игнорируем Ctrl+C в родительском процессе
    signal(SIGINT, SIG_IGN);

    // запоминаем код завершения помледней команды
    int last_status = 0;

    //запоминаем вводится ли команда с терминала
    int interactive = isatty(STDIN_FILENO);

    // при печати дерева приглашение не нужно
    if (dump_ast == 1 || dump_tokens == 1) {
        interactive = 0;
    }

    while (1) {

        // если не удалось вывести приглашение - завершаем
        if (interactive == 1 && print_prompt() == 0) {
            break;
        }
        
        int input_error;
        char *line = read_line(interactive, &input_error);

        // после Ctrl+D в терминале переходим на новую строку
        if (line == NULL) {
            if (input_error == 1) {
                return 1;
            }
            if (interactive == 1) {
                write(STDOUT_FILENO, "\n", 1);
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

        // выполнение && и || пока не подключено
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
        
        // одиночные команды выполняем через дерево
        if (tree->type == NODE_SIMPLE) {
            char **tree_arg = tree->arg;
            int my_command = 0;
            int end_shell = 0;

            if (strcmp(tree_arg[0], "cd") == 0 || 
            strcmp(tree_arg[0], "pwd") == 0 || 
            strcmp(tree_arg[0], "echo") == 0 || 
            strcmp(tree_arg[0], "help") == 0 ||
            strcmp(tree_arg[0], "exit") == 0){
                my_command = 1;
            }

            if (my_command == 1) {
                last_status = run_my_command_node(tree, last_status, &end_shell);
            } else {
                last_status = run_simple_node(tree);
            }

            free_tree(tree);
            free_tokens(tokens);
 
            if (end_shell == 1) {
                return last_status;
            }

            continue;
        }

        // конвейер выполняет по данным из дерева
        last_status = run_pipe_node(tree, last_status);
        free_tree(tree);
        free_tokens(tokens);
    }
    return last_status;
}