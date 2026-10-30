#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

Node *create_node(NodeType type) {
    Node *node = malloc(sizeof(Node));

    if (node == NULL) {
        perror("mysh: malloc");
        exit(1);
    }

    node->type = type;

    node->arg = NULL;
    node->arg_count = 0;
    node->left = NULL;
    node->right = NULL;
    node->redirects = NULL;

    return node;
}

static const char *node_type_name(NodeType type) {
    switch (type) {
        case NODE_SIMPLE:
            return "SIMPLE";

        case NODE_SUBSHELL:
            return "SUBSHELL";

        case NODE_PIPE:
            return "PIPE";

        case NODE_AND:
            return "AND";

        case NODE_OR:
            return "OR";

        case NODE_SEQUENCE:
            return "SEQUENCE";

        case NODE_BACKGROUND:
            return "BACKGROUND";
    }

    return "UNKNOWN";
}

static void print_space(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

static void free_redirects(Redirection *redirect) {
    while (redirect != NULL) {
        Redirection *next_redirect = redirect->next;

        free(redirect->file_name);
        free(redirect);

        redirect = next_redirect;
    }
}

void free_tree(Node *node) {
    if (node == NULL) {
        return;
    }

    free_tree(node->left);
    free_tree(node->right);

    for (size_t i = 0; i < node->arg_count; i++) {
        free(node->arg[i]);
    }
    
    free(node->arg);
    free_redirects(node->redirects);
    free(node);
}

void print_tree(const Node *node, int level) {
    if (node == NULL) {
        return;
    }

    print_space(level);
    printf("%s", node_type_name(node->type));

    if (node->type == NODE_SIMPLE) {
        for (size_t i = 0; i < node->arg_count; i++) {
            printf(" {%s}", node->arg[i]);

            if (i + 1 < node->arg_count) {
                printf(",");
            }
        }
    }

    printf("\n");

    print_tree(node->left, level + 1);
    print_tree(node->right, level + 1);
}