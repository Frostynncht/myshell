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
        
        char *line = read_line(interactive);

        // после Ctrl+D в терминале переходим на новую строку
        if (line == NULL) {
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
        
        // одиночные внешние команды выполняем через дерево
        if (tree->type == NODE_SIMPLE && strcmp(tree->arg[0], "exit") != 0) {
            char **tree_arg = tree->arg;
            int my_command = 0;

            if (strcmp(tree_arg[0], "cd") == 0 || 
            strcmp(tree_arg[0], "pwd") == 0 || 
            strcmp(tree_arg[0], "echo") == 0 || 
            strcmp(tree_arg[0], "help") == 0) {
                my_command = 1;
            }

            if (my_command == 1) {
                last_status = run_my_command_node(tree);
            } else {
                last_status = run_simple_node(tree);
            }

            free_tree(tree);
            free_tokens(tokens);
            continue;
        }
        // остальные команды пока передаём старому исполнителю
        free_tree(tree);

        // считает токены для массива arg
        size_t count = 0;
        while(tokens[count].type != TOKEN_END) {
            count++;
        }

        // создаем массив аргументов(text токена) для передачи в runner
        char **arg = malloc((count + 1) * sizeof(char *));
        if (arg == NULL) {
            perror("mysh: malloc");
            free_tokens(tokens);
            break;
        }

        for (size_t i = 0; i < count; i++) {
            arg[i] = tokens[i].text;
        }

        arg[count] = NULL;

        // проверка на наличие оператора пайпа
        int have_pipe = 0;

        for (size_t i = 0; i < count; i++) {
            if (tokens[i].type == TOKEN_PIPE) {
                have_pipe = 1;
                break;
            }
        }

        if (arg[0] == NULL) {
            free(arg);
            free_tokens(tokens);
            continue;
        }

        // без пайпа exit завершает сам шелл
        if (strcmp(arg[0], "exit") == 0 && have_pipe == 0) {
            int exit_status = last_status;
            if (arg[1] != NULL) {
                if (arg[2] != NULL) {
                    fprintf(stderr, "mysh: exit: too many arguments\n");
                    last_status = 2;
                    free(arg);
                    free_tokens(tokens);
                    continue;
                }

                const char *number_text = arg[1];
                int i = 0;
                int neg = 0;
                int valid = 1;
                int number = 0;

                // пропускаем знак числа(запоминаем если минус)
                if (number_text[i] == '+' || number_text[i] == '-') {
                    if (number_text[i] == '-') {
                        neg = 1;
                    }
                    i++;
                }

                if (number_text[i] == '\0') {
                    valid = 0;
                }

                // проверяем каждый символ и собираем число
                while (number_text[i] != '\0') {
                    if (number_text[i] < '0' || number_text[i] > '9') {
                        valid = 0;
                        break;
                    }

                    // в пределах [0; 255]
                    number = (number * 10 + (number_text[i] - '0')) % 256;
                    i++;
                }

                if (valid == 0) {
                    fprintf(stderr, "mysh: exit: numeric argument required\n");
                    last_status = 2;
                    free(arg);
                    free_tokens(tokens);
                    continue;
                }

                // переводим отрицательное в положительное [0; 255]
                if (neg == 1) {
                    number = (256 - number) % 256;
                }

                exit_status = number;
            }
            
            free(arg);
            free_tokens(tokens);
            return exit_status;
        }

        // исполняет остальные команды
        last_status = runner(arg, tokens);

        free(arg);
        free_tokens(tokens);
    }

    return last_status;
}