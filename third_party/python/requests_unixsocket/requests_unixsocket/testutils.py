"""
Utilities helpful for writing tests

Provides a UnixSocketServerThread that creates a running server, listening on a
newly created unix socket.

Example usage:

.. code-block:: python

    def test_unix_domain_adapter_monkeypatch():
        with UnixSocketServerThread() as usock_thread:
            with requests_unixsocket.monkeypatch('http+unix://'):
                urlencoded_usock = quote_plus(usock_process.usock)
                url = 'http+unix://%s/path/to/page' % urlencoded_usock
                r = requests.get(url)
"""

import logging
import os
import threading
import time
import uuid
import waitress


logger = logging.getLogger(__name__)


class KillThread(threading.Thread):
    def __init__(self, server, *args, **kwargs):
        super(KillThread, self).__init__(*args, **kwargs)
        self.server = server

    def run(self):
        time.sleep(1)
        logger.debug('Sleeping')
        self.server._map.clear()


class WSGIApp:
    server = None

    def __call__(self, environ, start_response):
        logger.debug('WSGIApp.__call__: Invoked for %s', environ['PATH_INFO'])
        logger.debug('WSGIApp.__call__: environ = %r', environ)
        status_text = '200 OK'
        response_headers = [
            ('X-Transport', 'unix domain socket'),
            ('X-Socket-Path', environ['SERVER_PORT']),
            ('X-Requested-Query-String', environ['QUERY_STRING']),
            ('X-Requested-Path', environ['PATH_INFO'])]
        body_bytes = b'Hello world!'
        start_response(status_text, response_headers)
        logger.debug(
            'WSGIApp.__call__: Responding with '
            'status_text = %r; '
            'response_headers = %r; '
            'body_bytes = %r',
            status_text, response_headers, body_bytes)
        return [body_bytes]


class UnixSocketServerThread(threading.Thread):
    def __init__(self, *args, **kwargs):
        super(UnixSocketServerThread, self).__init__(*args, **kwargs)
        self.usock = self.get_tempfile_name()
        self.server = None
        self.server_ready_event = threading.Event()

    def get_tempfile_name(self):
        # I'd rather use tempfile.NamedTemporaryFile but IDNA limits
        # the hostname to 63 characters and we'll get a "InvalidURL:
        # URL has an invalid label" error if we exceed that.
        args = (os.stat(__file__).st_ino, os.getpid(), uuid.uuid4().hex[-8:])
        return '/tmp/test_requests.%s_%s_%s' % args

    def run(self):
        logger.debug('Call waitress.serve in %r ...', self)
        wsgi_app = WSGIApp()
        server = waitress.create_server(wsgi_app, unix_socket=self.usock)
        wsgi_app.server = server
        self.server = server
        self.server_ready_event.set()
        server.run()

    def __enter__(self):
        logger.debug('Starting %r ...' % self)
        self.start()
        logger.debug('Started %r.', self)
        self.server_ready_event.wait()
        return self

    def __exit__(self, *args):
        self.server_ready_event.wait()
        if self.server:
            KillThread(self.server).start()
