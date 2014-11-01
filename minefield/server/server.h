#ifndef SERVER_H
#define SERVER_H

#include "minefield.h"
#include "picoev.h"
#include "request.h"
#include "time_cache.h"


extern uint64_t max_content_length;      //max_content_length
extern int client_body_buffer_size; //client_body_buffer_size
extern PyObject* current_client;
extern PyObject* timeout_error;

#endif
