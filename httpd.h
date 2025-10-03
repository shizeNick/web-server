#ifndef MYLIB_H

#define MYLIB_H

int srv_init(int port);
int cli_accept(int s);
int cli_conn(int s, int c);
char *read_file(char *path);
char *get_error();

#endif // !MYLIB_H
