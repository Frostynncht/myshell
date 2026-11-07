#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

typedef struct {
    const Token *tokens;
    size_t position;
} Parser;

// возвращает текущий токен
static const Token *cur_token(const Parser *parser) {
    return &parser->tokens[parser->position];
}

// проверяет тип токена с ожидаемым типом
static int check_type(const Parser *parser, TokenType type) {
    const Token *token = cur_token(parser);
    
    if (token->type == type) {
        return 1;
    }

    return 0;
}

// возвращает текущий токен и двигается к следующему
static const Token *advance(Parser *parser) {
    const Token *token = cur_token(parser);

    if (token->type != TOKEN_END) {
        parser->position++;
    }

    return token;
}

// проверяет если тип текущего токена совпал переходит к следующему
static int match_type(Parser *parser, TokenType type) {
    if (check_type(parser, type) == 0) {
        return 0;
    }

    advance(parser);
    return 1;
}

// проверяет является ли перенаправлением
static int is_redirect(TokenType type) {
    if (type == TOKEN_OUTPUT || type == TOKEN_INPUT || type == TOKEN_APPEND) {
        return 1;
    }

    return 0;
}

// добавляет перенаправление в конец списка узла команды
static void add_redirect(Node *node, TokenType type, const char *file_name) {
    Redirection *new_redirect = malloc(sizeof(Redirection));

    if (new_redirect == NULL) {
        perror("mysh: malloc");
        exit(1);
    }

    new_redirect->file_name = strdup(file_name);

    if (new_redirect->file_name == NULL) {
        perror("mysh: strdup");
        free(new_redirect);
        exit(1);
    }

    new_redirect->type = type;
    new_redirect->next = NULL;

    // если это первое перенаправление оно становится началом списка
    if (node->redirects == NULL) {
        node->redirects = new_redirect;
        return;
    }

    Redirection *last_redirect = node->redirects;

    // ищем последнее перенаправление в списке
    while (last_redirect->next != NULL) {
        last_redirect = last_redirect->next;
    }

    // добавляем перенаправление после последнего в списке
    last_redirect->next = new_redirect;
}

// обрабатывает перенаправление и добавляет его в дерево команд
static int parse_redirect(Parser *parser, Node *node) {
    TokenType type = cur_token(parser)->type;

    // пропуск >, >>, <
    advance(parser);

    if (check_type(parser, TOKEN_WORD) == 0) {
        fprintf(stderr, "mysh: expected file name after redirection\n");
        return 0;
    }

    // запоминаем имя файла и переходим к следующему токену
    const Token *file_token = advance(parser);

    add_redirect(node, type, file_token->text);

    return 1;
}

// копирует слово и добавляет его в массив аргументов
static void add_arg(Node *node, const char *text) {
    char *copy_text = strdup(text);

    if (copy_text == NULL) {
        perror("mysh: strdup");
        exit(1);
    }

    // добавляем место для аргумента и для NULL в конце 
    char **new_arg = realloc(node->arg, (node->arg_count + 2) * sizeof(char *));

    if (new_arg == NULL) {
        perror("mysh: realloc");
        free(copy_text);
        exit(1);
    }

    node->arg = new_arg;

    node->arg[node->arg_count] = copy_text;
    node->arg_count++;

    node->arg[node->arg_count] = NULL;
}

// разбирает простую команду с аргументами и перенаправлениями
static Node *parse_simple(Parser *parser) {
    Node *node = create_node(NODE_SIMPLE);
    int have_word = 0; // простая команда содержит хотя бы одно слово

    // обработка токенов пока они относятся к простой команде
    while (1) {
        TokenType type = cur_token(parser)->type;

        // обычное слово добавляем в аргументы команды
        if (type == TOKEN_WORD) {
            const Token *word_token = advance(parser);

            add_arg(node, word_token->text);
            have_word = 1;
            continue;
        }

        // перенаправление сохраняем отдельно от аргументов
        if (is_redirect(type)) {
            if (parse_redirect(parser, node) == 0) {
                free_tree(node);
                return NULL;
            }

            continue;
        }

        // если встретили другой оператор или TOKEN_END
        break;
    }

    if (have_word == 0) {
        fprintf(stderr, "mysh: expected command\n");
        free_tree(node);
        return NULL;
    }

    return node;

}

// соединяет команды оператором | в дерево
static Node *parse_pipe(Parser *parser) {
    Node *left = parse_simple(parser);

    if (left == NULL) {
        return NULL;
    }
    
    // после пайпа должна быть вторая команда
    while (match_type(parser, TOKEN_PIPE)) {
        Node *right = parse_simple(parser);

        if (right == NULL) {
            free_tree(left);
            return NULL;
        }

        Node *pipe_node = create_node(NODE_PIPE);

        pipe_node->left = left;
        pipe_node->right = right;
    
        // теперь слева все дерево вершиной которого новый пайп
        left = pipe_node;
    }

    return left;
}

// строит дерево команд из массива токенов
Node *parse_tokens(const Token *tokens) {
    Parser parser;
    parser.tokens = tokens;
    parser.position = 0;

    if (check_type(&parser, TOKEN_END)) {
        return NULL;
    }

    Node *tree = parse_pipe(&parser);
    if (tree == NULL) {
        return NULL;
    }

    // проверяем все ли токены были прочитаны
    if (check_type(&parser, TOKEN_END) == 0) {
        fprintf(stderr, "mysh: unexpected token %s\n", cur_token(&parser)->text);
        free_tree(tree);
        return NULL;
    }

    return tree;
}