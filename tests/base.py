from minefield import server
import time
import requests
import threading


class ServerRunner(object):

    def __init__(self, app, middleware=None):
        self.app = app
        self.middleware = middleware
        self.thread = None
        self._stop = False

    def run(self):
        self._stop = False
        self.thread = threading.Thread(target=self._run)
        self.thread.start()

    def _run(self):
        #print('start')
        server.listen(("0.0.0.0", 8000))
        def check():
            if self._stop:
                #print('shutdown')
                server.shutdown()
        server.set_watchdog(check)
        if self.middleware:
            server.run(self.middleware(self.app))
        else:
            server.run(self.app)

    def stop(self):
        self._stop = True
        self.thread.join()


def run_client(client=None, app=None, middleware=None):
    application = app()
    s = ServerRunner(application, middleware)
    s.run()
    time.sleep(0.2)
    try:
        res = client()
        env = application.environ
        return env, res
    finally:
        s.stop()


class BaseApp(object):

    environ = None

