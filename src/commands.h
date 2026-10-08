#ifndef COMMANDS_H
#define COMMANDS_H

int run_pwd(void);

char *get_curr_dir(void);

int run_cd(char **arg);

int run_echo(char **arg);

#endif