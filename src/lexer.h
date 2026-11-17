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

// состояние строки перед чтением продолжения
typedef enum {
    LINE_READY, // закончить чтение
    LINE_SLASH, // продолжить чтение после '\'
    LINE_QUOTE // незакрытая кавычка(продолжить чтение)
} LineState;

// получает состояние строки перед чтением продолжения
LineState get_line_state(const char *line);

// возвращает массив токенов
Token *split_line(const char *line);

// освобождает память(массив токенов)
void free_tokens(Token *tokens);

// выводит токены для проверки лексера
void print_tokens(const Token *tokens);

#endif