#ifndef RUNNER_H
#define RUNNER_H

#include "lexer.h"

// исполняет комманду, из полученного массива аргументов
int runner(char **arg, const Token *tokens);

#endif