#include "log.h"
#include <sys/file.h>

#define LOG_BUF_SIZE 1024 * 16

static PyObject *access_logger;
static PyObject *err_logger;

int
set_access_logger(PyObject *obj)
{
    if(access_logger != NULL){
        Py_DECREF(access_logger);
    }
    access_logger = obj;
    //Py_INCREF(access_logger);
    return 1;
}

int
set_err_logger(PyObject *obj)
{
    if(err_logger != NULL){
        Py_DECREF(err_logger);
    }
    err_logger = obj;
    //Py_INCREF(err_logger);
    return 1;
}

int
call_access_logger(PyObject *environ)
{
    PyObject *args = NULL, *res = NULL;

    if(access_logger){
        if(environ == NULL){
            environ = Py_None;
            Py_INCREF(environ);
        }

        DEBUG("call access logger %p", access_logger);
        args = PyTuple_Pack(1, environ);
        res = PyObject_CallObject(access_logger, args);
        Py_DECREF(args);
        Py_XDECREF(res);
        if(PyErr_Occurred()){
            PyErr_Print();
            PyErr_Clear();
        }
    }
    return 1;
}


int
call_error_logger(void)
{
    PyObject *exception = NULL, *v = NULL, *tb = NULL;
    PyObject *args = NULL, *res = NULL;

    if(err_logger){
        PyErr_Fetch(&exception, &v, &tb);
        if(exception == NULL){
            goto err;
        }
        PyErr_NormalizeException(&exception, &v, &tb);
        if(exception == NULL){
            goto err;
        }
        DEBUG("exc:%p val:%p tb:%p",exception, v, tb);
        /* PySys_SetObject("last_type", exception); */
        /* PySys_SetObject("last_value", v); */
        /* PySys_SetObject("last_traceback", tb); */
        
        if(v == NULL){
            v = Py_None;
            Py_INCREF(v);
        }
        if(tb == NULL){
            tb = Py_None;
            Py_INCREF(tb);
        }
        PyErr_Clear();

        args = PyTuple_Pack(3, exception, v, tb);
        if(args == NULL){
            PyErr_Print();
            goto err;
        }
        DEBUG("call error logger %p", err_logger);
        res = PyObject_CallObject(err_logger, args);
        Py_DECREF(args);
        Py_XDECREF(res);
        Py_XDECREF(exception);
        Py_XDECREF(v);
        Py_XDECREF(tb);
        if(res == NULL){
            PyErr_Print();
        }
    }else{
        PyErr_Print();
    }
err:
    PyErr_Clear();
    return 1;
}
