#ifndef CLIENT_H
#define CLIENT_H

#include "minefield.h"
#include "request.h"

typedef struct _client {
    int fd;
    char *remote_addr;
    int remote_port;

    char keep_alive;
    char read_header_complete;
    int complete;

    request *reading_req;
    request *current_req;
    request_queue *request_queue;

    //PyObject *environ;          // wsgi environ
    uint16_t status_code;            // response status code

    PyObject *http_status;      // response status line(PyBytes)
    PyObject *headers;          // http response headers
    PyObject *response;         // wsgi response object
    PyObject *response_iter;    // wsgi response object (iter)
    char http_minor;
    uint8_t header_done;            // header write status
    uint8_t chunked_response;     // use Transfer-Encoding: chunked
    uint8_t content_length_set;     // content_length_set flag
    uint64_t content_length;         // content_length
    uint64_t write_bytes;            // send body length
    void *bucket;               //write_data
    uint8_t response_closed;    //response closed flag
    uint8_t use_cork;     // use TCP_CORK
    size_t read_len;
    char read_buff[16*1024];
} client_t;

typedef struct {
    PyObject_HEAD
    client_t *client;
} ClientObject;

extern PyTypeObject ClientObjectType;

PyObject* ClientObject_New(client_t* client);

int CheckClientObject(PyObject *obj);
void ClientObject_list_fill(void);
void ClientObject_list_clear(void);

#endif
