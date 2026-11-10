#ifndef RUNNER_H
#define RUNNER_H

#include "lexer.h"
#include "ast.h"

// исполняет простую внешнюю команду из узла дерева 
int run_simple_node(const Node *node);

// исполняет комманду, из полученного массива аргументов
int runner(char **arg, const Token *tokens);

int run_my_command_node(const Node *node);

#endif