#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include "client.h"

int init_parser(client_t *cli, const char *name, const short port);

int execute_parse(client_t *cli, int len);

void setup_static_env(char *name, int port);

void clear_static_env(void);

PyObject* new_environ(client_t *client);

#endif
