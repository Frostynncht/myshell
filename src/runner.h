#ifndef RUNNER_H
#define RUNNER_H

#include "lexer.h"
#include "ast.h"

// исполняет простую внешнюю команду из узла дерева 
int run_simple_node(const Node *node, int interactive);

// выполняет конвейер из дерева команд
int run_pipe_node(const Node *tree, int previous_status, int interactive);

int run_my_command_node(const Node *node, int previous_status, int *end_shell);

// выполняет дерево через end_shell сообщает о команде exit
int run_tree(const Node *node, int previous_status, int *end_shell, int interactive);

#endif