What's this
---------------------------------

.. image:: https://travis-ci.org/methane/minefield.svg
    :target: https://travis-ci.org/methane/minefield

This is a fork of `meinheld <http://github.com/mopemope/meinheld>`_, the
high performance python wsgi web server.

Minefield removes asynchronous feature from meinheld and
have some experimental tuning.

And minefield is a WSGI compliant web server. (PEP333 and PEP3333 supported)


Requirements
---------------------------------

minefield requires **Python 2.x >= 2.7** or **Python 3.x >= 3.3**.

minefield supports Linux, FreeBSD, Mac OS X.

Installation
---------------------------------

Install from pypi::

  $ pip install -U minefield

Install from source:: 

  $ python setup.py install

minefield supports gunicorn.

To install gunicorn::

  $ pip install -U gunicorn


Basic Usage
---------------------------------

simple wsgi app::

    from minefield import server

    def hello_world(environ, start_response):
        status = '200 OK'
        res = b"Hello world!"
        response_headers = [('Content-type', 'text/plain'), ('Content-Length', str(len(res)))]
        start_response(status, response_headers)
        return [res]

    server.listen(("0.0.0.0", 8000))
    server.run(hello_world)


with gunicorn. user worker class "egg:minefield#gunicorn_worker" or "minefield.gminefield.MinefieldWorker"::
    
    $ gunicorn --workers=2 --worker-class="egg:minefield#gunicorn_worker" gunicorn_test:app

Performance
------------------------------

For parsing HTTP requests, meinheld uses Ryan Dahl's http-parser library.

(see https://github.com/joyent/http-parser)

It is built around the high performance event library picoev.

(see http://developer.cybozu.co.jp/kazuho/2009/08/picoev-a-tiny-e.html)

sendfile
===========================

Meinheld uses sendfile(2), over wgsi.file_wrapper.
