from minefield import server
import time


def hello_world(environ, start_response):
    status = '200 OK'
    res = b"Hello world!"
    response_headers = [('Content-type','text/plain')]
    start_response(status, response_headers)
    print(environ)
    return [res]

def watchdog():
    print(time.time())

server.set_watchdog(watchdog)
server.listen(("0.0.0.0", 8000))
server.set_access_logger(None)
server.set_error_logger(None)
server.run(hello_world)


