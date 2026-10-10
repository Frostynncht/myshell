#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_WORD, // слово
    TOKEN_PIPE, // |
    TOKEN_OR, // ||
    TOKEN_AMPERSAND, // &
    TOKEN_AND, // &&
    TOKEN_SEMICOLON, // ;
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_INPUT, // <
    TOKEN_OUTPUT, // >
    TOKEN_APPEND, // >>
    TOKEN_END // конец массива
} TokenType;

typedef struct {
    TokenType type;
    char *text;
} Token;

Token *split_line(const char *line); // возвращает массив аргументов

void free_tokens(Token *tokens); // освобождает память(массив токенов)

#endif