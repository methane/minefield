#include "http_request_parser.h"
#include "server.h"
#include "response.h"
#include "input.h"
#include "util.h"
#include "picohttpparser/picohttpparser.h"

#define MAXFREELIST 1024

/**
 * environ spec.
 *
 * REQUEST_METHOD
 *   The HTTP request method, such as "GET" or "POST". This cannot ever be an empty string, and so is always required.
 *
 * SCRIPT_NAME
 *   The initial portion of the request URL's "path" that corresponds to the application object,
 *   so that the application knows its virtual "location".
 *   This may be an empty string, if the application corresponds to the "root" of the server.
 *
 * PATH_INFO
 *   The remainder of the request URL's "path", designating the virtual "location" of the request's target within the application.
 *   This may be an empty string, if the request URL targets the application root and does not have a trailing slash.
 *
 * QUERY_STRING
 *   The portion of the request URL that follows the "?", if any. May be empty or absent.
 *
 * CONTENT_TYPE
 *   The contents of any Content-Type fields in the HTTP request. May be empty or absent.
 *
 * CONTENT_LENGTH
 *   The contents of any Content-Length fields in the HTTP request. May be empty or absent.
 *
 * SERVER_NAME, SERVER_PORT
 *    When combined with SCRIPT_NAME and PATH_INFO, these variables can be used to complete the URL. Note, however, that HTTP_HOST,
 *    if present, should be used in preference to SERVER_NAME for reconstructing the request URL.
 *    See the URL Reconstruction section below for more detail.
 *    SERVER_NAME and SERVER_PORT can never be empty strings, and so are always required.
 *
 * SERVER_PROTOCOL
 *   The version of the protocol the client used to send the request.
 *   Typically this will be something like "HTTP/1.0" or "HTTP/1.1" and may be used by the application to determine
 *   how to treat any HTTP request headers.
 *   (This variable should probably be called REQUEST_PROTOCOL, since it denotes the protocol used in the request,
 *   and is not necessarily the protocol that will be used in the server's response.
 *   However, for compatibility with CGI we have to keep the existing name.)
 *
 * HTTP_ Variables
 *   Variables corresponding to the client-supplied HTTP request headers (i.e., variables whose names begin with "HTTP_").
 *   The presence or absence of these variables should correspond with the presence or absence of the appropriate
 *   HTTP header in the request.
 *
 */

static int prefix_len;

static PyObject *empty_string;

static PyObject *version_key;
static PyObject *version_val;
static PyObject *scheme_key;
static PyObject *scheme_val;
static PyObject *errors_key;
static PyObject *errors_val;
static PyObject *multithread_key;
static PyObject *multithread_val;
static PyObject *multiprocess_key;
static PyObject *multiprocess_val;
static PyObject *run_once_key;
static PyObject *run_once_val;
static PyObject *file_wrapper_key;
static PyObject *file_wrapper_val;
static PyObject *wsgi_input_key;

static PyObject *script_key;
static PyObject *server_name_key;
static PyObject *server_name_val;
static PyObject *server_port_key;
static PyObject *server_port_val;
static PyObject *remote_addr_key;
static PyObject *remote_port_key;

static PyObject *server_protocol_key;
static PyObject *path_info_key;
static PyObject *query_string_key;
static PyObject *request_method_key;
static PyObject *client_key;

static PyObject *content_type_key;
static PyObject *content_length_key;

static PyObject *server_protocol_val10;
static PyObject *server_protocol_val11;

static PyObject *http_method_delete;
static PyObject *http_method_get;
static PyObject *http_method_head;
static PyObject *http_method_post;
static PyObject *http_method_put;
static PyObject *http_method_patch;
static PyObject *http_method_connect;
static PyObject *http_method_options;
static PyObject *http_method_trace;
static PyObject *http_method_copy;
static PyObject *http_method_lock;
static PyObject *http_method_mkcol;
static PyObject *http_method_move;
static PyObject *http_method_propfind;
static PyObject *http_method_proppatch;
static PyObject *http_method_unlock;
static PyObject *http_method_report;
static PyObject *http_method_mkactivity;
static PyObject *http_method_checkout;
static PyObject *http_method_merge;

PyObject*
new_environ(client_t *client)
{
    PyObject *object, *environ;

    environ = PyDict_New();
    PyDict_SetItem(environ, version_key, version_val);
    PyDict_SetItem(environ, scheme_key, scheme_val);
    PyDict_SetItem(environ, errors_key, errors_val);
    PyDict_SetItem(environ, multithread_key, multithread_val);
    PyDict_SetItem(environ, multiprocess_key, multiprocess_val);
    PyDict_SetItem(environ, run_once_key, run_once_val);
    PyDict_SetItem(environ, script_key, empty_string);
    PyDict_SetItem(environ, server_name_key, server_name_val);
    PyDict_SetItem(environ, server_port_key, server_port_val);
    PyDict_SetItem(environ, file_wrapper_key, file_wrapper_val);

    object = NATIVE_FROMSTRING(client->remote_addr);
    PyDict_SetItem(environ, remote_addr_key, object);
    Py_DECREF(object);

    object = NATIVE_FROMFORMAT("%d", client->remote_port);
    PyDict_SetItem(environ, remote_port_key, object);
    Py_DECREF(object);
    return environ;
}

static int
hex2int(int i)
{
    i = toupper(i);
    i = i - '0';
    if(i > 9){
        i = i - 'A' + '9' + 1;
    }
    return i;
}

static int
urldecode(char *buf, int len)
{
    int c, c1;
    char *s0, *t;
    t = s0 = buf;
    while(len >0){
        c = *buf++;
        if(c == '%' && len > 2){
            c = *buf++;
            c1 = c;
            c = *buf++;
            c = hex2int(c1) * 16 + hex2int(c);
            len -= 2;
        }
        *t++ = c;
        len--;
    }
    *t = 0;
    return t - s0;
}

static int
set_query(PyObject *env, char *buf, int len)
{
    int c, ret, slen = 0;
    char *s0;
    PyObject *obj;
    s0 = buf;
    while(len > 0){
        c = *buf++;
        if(c == '#'){
            slen++;
            break;
        }
        // query       = *( pchar / "/" / "?" )
        // unreserved  = ALPHA / DIGIT / "-" / "." /       "_" / "~"
        if (c >= 127) {
            return 400;  // Bad request
        }
        len--;
        slen++;
    }

    if(slen > 1){
#if PY3
        obj = PyUnicode_FromStringAndSize(s0, slen-1);
#else
        obj = PyBytes_FromStringAndSize(s0, slen -1);
#endif
        /* DEBUG("query:%.*s", len, PyBytes_AS_STRING(obj)); */
        if(unlikely(obj == NULL)){
            return 500;
        }
        ret = PyDict_SetItem(env, query_string_key, obj);
        Py_DECREF(obj);
        if(unlikely(ret == -1)){
            return 500;
        }
    }
    return 0; 
}


static int
set_path(PyObject *env, char *buf, int len)
{
    int c, c1, slen;
    char *s0, *t;
    PyObject *obj;

    t = s0 = buf;
    while(len > 0){
        c = *buf++;
        if(c == '%' && len > 2){
            c = *buf++;
            c1 = c;
            c = *buf++;
            c = hex2int(c1) * 16 + hex2int(c);
            len -= 2;
        }else if(c == '?'){
            //stop
            int ret = set_query(env, buf, len);
            if (ret != 0)
                return ret;
            break;
        }else if(c == '#'){
            //stop 
            //ignore fragment
            break;
        }
        *t++ = c;
        len--;
    }
    //*t = 0;
    slen = t - s0;
    slen = urldecode(s0, slen);

#if PY3
    obj = PyUnicode_DecodeUTF8(s0, slen);
#else
    obj = PyBytes_FromStringAndSize(s0, slen);
#endif
    /* DEBUG("path:%.*s", (int)slen, PyBytes_AS_STRING(obj)); */
    if(likely(obj != NULL)){
        PyDict_SetItem(env, path_info_key, obj);
        Py_DECREF(obj);
        return 0;
    }else{
        return 500; // internal server error
    }
}

static PyObject*
get_http_header_key(const char *s, int len)
{
    PyObject *obj;
    char *dest;

#if PY3
    obj = PyUnicode_New(len + prefix_len, 127);
    if (unlikely(obj == NULL)) return NULL;
    dest = (char*)PyUnicode_1BYTE_DATA();
#else
    obj = PyBytes_FromStringAndSize(NULL, len + prefix_len);
    if (unlikely(obj == NULL)) return NULL;
    dest = (char*)PyBytes_AS_STRING(obj);
#endif

    *dest++ = 'H';
    *dest++ = 'T';
    *dest++ = 'T';
    *dest++ = 'P';
    *dest++ = '_';
    memcpy(dest, s, len);
    return obj;
}

static int64_t
get_content_length(const char *s, size_t len)
{
    int64_t clen = 0;
    while (len--) {
        char c = *s++;
        if (c < '0' || c > '9') return -1;
        clen = clen * 10 + (c - '0');
    }
    return clen;
}

static int
key_upper(char *s, size_t len)
{
    while (len) {
        char c = *s;
        if (unlikely(c < 0x21 || c > 0x7E)) { // VCHAR in RFC5234
            return 400;
        } else if (c == '-') {
            *s = '_';
        } else if (c >= 'a' && c <= 'z') {
            *s = c - ('a' - 'A');
        }
        s++;
        len--;
    }
    return 0;
}

static int
write_body2file(request *req, const char *buffer, size_t buffer_len)
{
    FILE *tmp = (FILE *)req->body;
    fwrite(buffer, 1, buffer_len, tmp);
    req->body_readed += buffer_len;
    DEBUG("write_body2file %d bytes", (int)buffer_len);
    return req->body_readed;

}

static int
write_body2mem(request *req, const char *buf, size_t buf_len)
{
    buffer_t *body = (buffer_t*)req->body;
    write2buf(body, buf, buf_len);

    req->body_readed += buf_len;
    DEBUG("write_body2mem %d bytes", (int)buf_len);
    return req->body_readed;
}

static int
write_body(request *req, const char *buffer, size_t buffer_len)
{
    if (req->body_type == BODY_TYPE_TMPFILE) {
        return write_body2file(req, buffer, buffer_len);
    } else {
        return write_body2mem(req, buffer, buffer_len);
    }
}

static int
message_begin_cb(client_t *client)
{
    request *req = NULL;
    PyObject *environ = NULL;

    DEBUG("message_begin_cb");

    req = new_request();
    if (req == NULL) return -1;

    req->start_msec = current_msec;
    client->reading_req = req;
    environ = new_environ(client);
    /* client->bad_request_code = 0; */
    /* client->body_type = BODY_TYPE_NONE; */
    /* client->body_readed = 0; */
    /* client->body_length = 0; */
    req->environ = environ;
    return 0;
}

static int
body_cb(request *req, const char *buf, size_t len)
{
    DEBUG("body_cb");

    if(max_content_length < req->body_readed + len){
        DEBUG("set request code %d", 413);
        req->bad_request_code = 413;
        return -1;
    }
    if(req->body_type == BODY_TYPE_NONE){
        if(req->body_length == 0){
            //Length Required
            req->bad_request_code = 411;
            return -1;
        }
        if(req->body_length > client_body_buffer_size){
            //large size request
            FILE *tmp = tmpfile();
            if(tmp < 0){
                req->bad_request_code = 500;
                return -1;
            }
            req->body = tmp;
            req->body_type = BODY_TYPE_TMPFILE;
            DEBUG("BODY_TYPE_TMPFILE");
        }else{
            //default memory stream
            DEBUG("client->body_length %d", req->body_length);
            req->body = new_buffer(req->body_length, 0);
            req->body_type = BODY_TYPE_BUFFER;
            DEBUG("BODY_TYPE_BUFFER");
        }
    }
    write_body(req, buf, len);
    return 0;
}

static int
parse_header(client_t *client, size_t len)
{
    PyObject *obj;
    int ret, nread, n;
    int should_keep_alive;
    int has_content_length = 0;
    int content_length = 0;

    const char *method;
    size_t method_len;
    const char *path;
    size_t path_len;
    int minor_version;
    struct phr_header headers[64];
    size_t num_headers=64;

    request *req = client->reading_req;
    PyObject *env = req->environ;

    nread = phr_parse_request(client->read_buff, len, &method, &method_len,
                              &path, &path_len, &minor_version,
                              headers, &num_headers, client->read_len);
    if (nread < 0) return nread;

    should_keep_alive = minor_version == 1 ? 1 : 0;
    client->http_minor = minor_version;

    for (n=0; n<num_headers; n++) {
        PyObject *field, *value;
        int ret = key_upper((char*)headers[n].name, headers[n].name_len);
        if (ret != 0) {
            req->bad_request_code = ret;
            return -1;
        }

        if (headers[n].name_len == 12 && strncmp(headers[n].name, "CONTENT_TYPE", 12) == 0) {
            field = content_type_key;
            Py_INCREF(field);
        } else if (headers[n].name_len == 14 && strncmp(headers[n].name, "CONTENT_LENGTH", 14) == 0) {
            content_length = get_content_length(headers[n].value, headers[n].value_len);
            if (content_length < 0) {
                req->bad_request_code = 400;
                return -1;
            }
            has_content_length = 1;
            field = content_length_key;
            Py_INCREF(field);
        } else {
            if (headers[n].name_len == 10 && strncmp(headers[n].name, "CONNECTION", 10) == 0) {
                if (headers[n].value_len == 10 && strncasecmp(headers[n].value, "keep-alive", 10) == 0) {
                    should_keep_alive = 1;
                } else if (headers[n].value_len == 5 && strncasecmp(headers[n].value, "close", 5) == 0) {
                    should_keep_alive = 0;
                } else {
                    req->bad_request_code = 400;
                    return -1;
                }
            }
            field = get_http_header_key(headers[n].name, headers[n].name_len);
            if (unlikely(field == NULL)) {
                req->bad_request_code = 500;
                return -1;
            }
        }
#if PY3
        value = PyUnicode_DecodeLatin1(headers[n].value, headers[n].value_len);
#else
        value = PyBytes_FromStringAndSize(headers[n].value, headers[n].value_len);
#endif
        if (unlikely(value == NULL)) {
            Py_DECREF(field);
            req->bad_request_code = 500;
            return -1;
        }
        ret = PyDict_SetItem(env, field, value);
        Py_CLEAR(field);
        Py_CLEAR(value);
        if (unlikely(ret != 0)) {
            RDEBUG("failed to PyDict_SetItem()");
            req->bad_request_code = 500;
            return -1;
        }
    }

    //if (!has_content_length) {
    //    // Content Length Required.
    //    req->bad_request_code = 411;
    //    return -1;
    //}
    if (content_length > max_content_length) {
        RDEBUG("max_content_length over %d/%d", (int)content_length, (int)max_content_length);
        DEBUG("set request code %d", 413);
        req->bad_request_code = 413;
        return -1;
    }

    DEBUG("should keep alive %d", should_keep_alive);
    client->keep_alive = should_keep_alive;

    if (minor_version == 1) {
        obj = server_protocol_val11;
    } else {
        obj = server_protocol_val10;
    }
    ret = PyDict_SetItem(env, server_protocol_key, obj);
    if (unlikely(ret == -1))
        return -1;

    if (*path != '/') {
        //TODO: support absolute-URI
        req->bad_request_code = 400;
        return -1;
    }
    ret = set_path(env, (char*)path, path_len);
    if(unlikely(ret != 0)){
        req->bad_request_code = ret;
        return -1;
    }

    key_upper((char*)method, method_len);
    obj = NULL;
    switch (method_len) {
    case 3:
        if (0 ==      strncmp(method, "GET", 3))
            obj = http_method_get;
        else if (0 == strncmp(method, "PUT", 3))
            obj = http_method_put;
        break;
    case 4:
        if (0 ==      strncmp(method, "HEAD", 4))
            obj = http_method_head;
        else if (0 == strncmp(method, "POST", 4))
            obj = http_method_post;
        else if (0 == strncmp(method, "COPY", 4))
            obj = http_method_copy;
        else if (0 == strncmp(method, "LOCK", 4))
            obj = http_method_lock;
        else if (0 == strncmp(method, "MOVE", 4))
            obj = http_method_move;
    case 5:
        if (0 ==      strncmp(method, "PATCH", 5))
            obj = http_method_patch;
        else if (0 == strncmp(method, "TRACE", 5))
            obj = http_method_trace;
        else if (0 == strncmp(method, "MKCOL", 5))
            obj = http_method_mkcol;
        else if (0 == strncmp(method, "MERGE", 5))
            obj = http_method_merge;
    case 6:
        if (0 ==      strncmp(method, "DELETE", 6))
            obj = http_method_delete;
        else if (0 == strncmp(method, "UNLOCK", 6))
            obj = http_method_unlock;
        else if (0 == strncmp(method, "REPORT", 6))
            obj = http_method_report;
    case 7:
        if (0 ==      strncmp(method, "CONNECT", 7))
            obj = http_method_connect;
        else if (0 == strncmp(method, "OPTIONS", 7))
            obj = http_method_options;
    case 8:
        if (0 ==      strncmp(method, "PROPFIND", 8))
            obj = http_method_propfind;
        else if (0 == strncmp(method, "CHECKOUT", 8))
            obj = http_method_checkout;
    case 9:
        if (0 == strncmp(method, "PROPPATCH", 9))
            obj = http_method_proppatch;
    case 10:
        if (0 == strncmp(method, "MKACTIVITY", 10))
            obj = http_method_mkactivity;
    }
    if (unlikely(obj == NULL)) {
#if PY3
        obj = PyUnicode_FromStringAndSize(method, method_len);
#else
        obj = PyBytes_FromStringAndSize(method, method_len);
#endif
        ret = PyDict_SetItem(env, request_method_key, obj);
        Py_DECREF(obj);
    } else {
        ret = PyDict_SetItem(env, request_method_key, obj);
    }
    obj = NULL;
    if(unlikely(ret == -1)){
        return -1;
    }
    req->body_length = content_length;

    //keep client data
    obj = ClientObject_New(client);
    if (unlikely(obj == NULL)){
        return -1;
    }
    ret = PyDict_SetItem(env, client_key, obj);
    Py_DECREF(obj);
    if (unlikely(ret < 0)) return ret;
    return nread;
}

static int
message_complete(client_t *client)
{
    DEBUG("message_complete");
    client->complete++;
    push_request(client->request_queue, client->reading_req);
    client->reading_req = NULL;
    return 0;
}

static PyMethodDef method = {"file_wrapper", (PyCFunction)file_wrapper, METH_VARARGS, 0};

int
init_parser(client_t *cli, const char *name, const short port)
{
    cli->keep_alive = 0;
    cli->read_len = 0;
    cli->complete = 0;
    return 0;
}

/** Parse request.
 *
 * @return -1 on error, -2 on incomplete and 0 for complete.
 */
int
execute_parse(client_t *cli, int len)
{
    int ret = 0;
    char *buf = 0;
    int remain;
retry:
    buf = cli->read_buff;
    request *request = cli->reading_req;

    if (request == NULL) {
        // First parse for this request. Prepare request object.
        ret = message_begin_cb(cli);
        if (ret < 0)
            return ret;
        request = cli->reading_req;
        cli->read_header_complete = 0;
    }

    if (!cli->read_header_complete) {
        ret = parse_header(cli, cli->read_len + len);
        cli->read_len += len;  // Used next try when ret == INCOMPLETE
        if (ret < 0)
            return ret;

        cli->read_header_complete = 1;
        buf = cli->read_buff + ret;
        len = cli->read_len - ret;
    }

    remain = request->body_length - (len + request->body_readed);
    if (remain < 0) {
        // Next request is pipelined.
        len += remain;
    }
    if (request->body_length > 0) {
        ret = body_cb(request, buf, len);
        if (ret < 0) {
            return ret;
        }
        request->body_readed += len;
        if (remain > 0) {
            return -2; // incomplete
        }
    }
    message_complete(cli);

    if (remain < 0) {
        DEBUG("request pipelined. remains %d, offs %d, len %d", -remain, buf-cli->read_buff, len);
        //TODO: prepare to next request.
        memmove(cli->read_buff, buf+len, -remain);
        cli->read_len = 0;
        len = -remain;
        DEBUG("%.*s", len, cli->read_buff);
        goto retry;
    }
    return 0;
}


void
setup_static_env(char *name, int port)
{
    prefix_len = strlen("HTTP_");

    empty_string = NATIVE_FROMSTRING("");

    version_val = Py_BuildValue("(ii)", 1, 0);
    version_key = NATIVE_FROMSTRING("wsgi.version");

    scheme_val = NATIVE_FROMSTRING("http");
    scheme_key = NATIVE_FROMSTRING("wsgi.url_scheme");

    errors_val = PySys_GetObject("stderr");
    errors_key = NATIVE_FROMSTRING("wsgi.errors");

    multithread_val = PyBool_FromLong(0);
    multithread_key = NATIVE_FROMSTRING("wsgi.multithread");

    multiprocess_val = PyBool_FromLong(1);
    multiprocess_key = NATIVE_FROMSTRING("wsgi.multiprocess");

    run_once_val = PyBool_FromLong(0);
    run_once_key = NATIVE_FROMSTRING("wsgi.run_once");

    file_wrapper_val = PyCFunction_New(&method, NULL);
    file_wrapper_key = NATIVE_FROMSTRING("wsgi.file_wrapper");

    wsgi_input_key = NATIVE_FROMSTRING("wsgi.input");
    
    script_key = NATIVE_FROMSTRING("SCRIPT_NAME");

    server_name_val = NATIVE_FROMSTRING(name);
    server_name_key = NATIVE_FROMSTRING("SERVER_NAME");

    server_port_val = NATIVE_FROMFORMAT("%d", port);
    server_port_key = NATIVE_FROMSTRING("SERVER_PORT");

    remote_addr_key = NATIVE_FROMSTRING("REMOTE_ADDR");
    remote_port_key = NATIVE_FROMSTRING("REMOTE_PORT");

    server_protocol_key = NATIVE_FROMSTRING("SERVER_PROTOCOL");
    path_info_key = NATIVE_FROMSTRING("PATH_INFO");
    query_string_key = NATIVE_FROMSTRING("QUERY_STRING");
    request_method_key = NATIVE_FROMSTRING("REQUEST_METHOD");
    client_key = NATIVE_FROMSTRING("minefield.client");

    content_type_key = NATIVE_FROMSTRING("CONTENT_TYPE");
    content_length_key = NATIVE_FROMSTRING("CONTENT_LENGTH");

    server_protocol_val10 = NATIVE_FROMSTRING("HTTP/1.0");
    server_protocol_val11 = NATIVE_FROMSTRING("HTTP/1.1");

    http_method_delete = NATIVE_FROMSTRING("DELETE");
    http_method_get = NATIVE_FROMSTRING("GET");
    http_method_head = NATIVE_FROMSTRING("HEAD");
    http_method_post = NATIVE_FROMSTRING("POST");
    http_method_put = NATIVE_FROMSTRING("PUT");
    http_method_patch = NATIVE_FROMSTRING("PATCH");
    http_method_connect = NATIVE_FROMSTRING("CONNECT");
    http_method_options = NATIVE_FROMSTRING("OPTIONS");
    http_method_trace = NATIVE_FROMSTRING("TRACE");
    http_method_copy = NATIVE_FROMSTRING("COPY");
    http_method_lock = NATIVE_FROMSTRING("LOCK");
    http_method_mkcol = NATIVE_FROMSTRING("MKCOL");
    http_method_move = NATIVE_FROMSTRING("MOVE");
    http_method_propfind= NATIVE_FROMSTRING("PROPFIND");
    http_method_proppatch = NATIVE_FROMSTRING("PROPPATCH");
    http_method_unlock = NATIVE_FROMSTRING("UNLOCK");
    http_method_report = NATIVE_FROMSTRING("REPORT");
    http_method_mkactivity = NATIVE_FROMSTRING("MKACTIVITY");
    http_method_checkout = NATIVE_FROMSTRING("CHECKOUT");
    http_method_merge = NATIVE_FROMSTRING("MERGE");

    //PycString_IMPORT;
}

void
clear_static_env(void)
{
    DEBUG("clear_static_env");
    Py_DECREF(empty_string);

    Py_DECREF(version_key);
    Py_DECREF(version_val);
    Py_DECREF(scheme_key);
    Py_DECREF(scheme_val);
    Py_DECREF(errors_key);
    /* Py_DECREF(errors_val); */
    Py_DECREF(multithread_key);
    Py_DECREF(multithread_val);
    Py_DECREF(multiprocess_key);
    Py_DECREF(multiprocess_val);
    Py_DECREF(run_once_key);
    Py_DECREF(run_once_val);
    Py_DECREF(file_wrapper_key);
    Py_DECREF(file_wrapper_val);
    Py_DECREF(wsgi_input_key);

    Py_DECREF(script_key);
    Py_DECREF(server_name_key);
    Py_DECREF(server_name_val);
    Py_DECREF(server_port_key);
    Py_DECREF(server_port_val);
    Py_DECREF(remote_addr_key);
    Py_DECREF(remote_port_key);

    Py_DECREF(server_protocol_key);
    Py_DECREF(path_info_key);
    Py_DECREF(query_string_key);
    Py_DECREF(request_method_key);
    Py_DECREF(client_key);

    Py_DECREF(content_type_key);
    Py_DECREF(content_length_key);

    Py_DECREF(server_protocol_val10);
    Py_DECREF(server_protocol_val11);

    Py_DECREF(http_method_delete);
    Py_DECREF(http_method_get);
    Py_DECREF(http_method_head);
    Py_DECREF(http_method_post);
    Py_DECREF(http_method_put);
    Py_DECREF(http_method_patch);
    Py_DECREF(http_method_connect);
    Py_DECREF(http_method_options);
    Py_DECREF(http_method_trace);
    Py_DECREF(http_method_copy);
    Py_DECREF(http_method_lock);
    Py_DECREF(http_method_mkcol);
    Py_DECREF(http_method_move);
    Py_DECREF(http_method_propfind);
    Py_DECREF(http_method_proppatch);
    Py_DECREF(http_method_unlock);
    Py_DECREF(http_method_report);
    Py_DECREF(http_method_mkactivity);
    Py_DECREF(http_method_checkout);
    Py_DECREF(http_method_merge);
}
