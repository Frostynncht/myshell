#ifndef COMMANDS_H
#define COMMANDS_H

int run_pwd(void);

char *get_curr_dir(void);

int run_cd(char **arg);

int run_echo(char **arg);

int run_help(void);

int get_exit_code(char **arg, int last_status, int *exit_status);

#endif