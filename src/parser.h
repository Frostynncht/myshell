#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// строит дерево команд из массива токенов
Node *parse_tokens(const Token *tokens);

#endif