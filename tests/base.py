from minefield import server
import time
import requests
import threading


class ClientRunner(object):

    def __init__(self, app, middleware=None):
        self.app = app
        self.middleware = middleware

    def run(self, client):
        self.stop = False
        self.started = False

        def run_client():
            while not self.started:
                time.sleep(0.01)
            try:
                self.result = client()
            except Exception as e:
                self.result = e
            finally:
                self.env = self.app.environ
                self.stop = True

        thread = threading.Thread(target=run_client)
        thread.start()

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

        thread.join()
        return self.env, self.result


def run_client(client=None, app=None, middleware=None):
    application = app()
    s = ClientRunner(application, middleware)
    return s.run(client)


class BaseApp(object):

    environ = None

