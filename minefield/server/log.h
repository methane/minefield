#ifndef LOG_H
#define LOG_H

#include "client.h"
#include "time_cache.h"

int set_access_logger(PyObject *obj);
int set_err_logger(PyObject *obj);

int call_access_logger(PyObject *environ);
int call_error_logger(void);


#endif
