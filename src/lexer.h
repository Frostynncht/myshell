#ifndef LEXER_H
#define LEXER_H


char **split_line(char *line); // возвращает массив аргументов

void free_arg(char **arg); // освобождает память(массив аргументов)

#endif