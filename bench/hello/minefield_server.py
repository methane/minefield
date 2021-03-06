from minefield import server

def hello_world(environ, start_response):
    status = '200 OK'
    res = b"Hello world!"
    response_headers = [('Content-type','text/plain')]
    start_response(status, response_headers)
    return [res]

server.listen(("0.0.0.0", 8000))
server.set_keepalive(20)
server.set_access_logger(None)
server.set_error_logger(None)
server.run(hello_world)
