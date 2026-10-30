#ifndef AST_H
#define AST_H

#include <stddef.h>
#include "lexer.h"

typedef enum {
    NODE_SIMPLE, // простая команда
    NODE_SUBSHELL, // команда внутри скобок ()
    NODE_PIPE, // |
    NODE_AND, // &&
    NODE_OR, // ||
    NODE_SEQUENCE, // ;
    NODE_BACKGROUND // &
} NodeType;

// перенаправление и указатель на следующее
typedef struct Redirection {
    TokenType type;
    char *file_name;
    struct Redirection *next;
} Redirection;

// узел дерева команд
typedef struct Node {
    NodeType type;

    char **arg;
    size_t arg_count;
    Redirection *redirects;

    struct Node *left;
    struct Node *right;
} Node;

// выделяет память и создает пустой узел выбранного типа
Node *create_node(NodeType type);

// освобождает узел и его поддерево
void free_tree(Node *node);

// выводит дерево(для проверки парсера)
void print_tree(const Node *node, int level);

#endif