#ifndef INPUT_H
#define INPUT_H

// читает команду, interactive - нужно ли печатать приглашение продолжения, при ошибке записывает 1 в input_error 
char *read_line(int interactive, int *input_error);

// читает очередную команду из текста и двигает указатель
char *read_command(const char **text, int *input_error);

int print_prompt(void);

#endif 