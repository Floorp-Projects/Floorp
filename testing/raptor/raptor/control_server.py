# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# control server for raptor performance framework
# communicates with the raptor browser webextension
from __future__ import absolute_import

import BaseHTTPServer
import json
import os
import socket
import threading

from mozlog import get_proxy_logger

LOG = get_proxy_logger(component='raptor-control-server')

here = os.path.abspath(os.path.dirname(__file__))


def MakeCustomHandlerClass(results_handler, shutdown_browser):

    class MyHandler(BaseHTTPServer.BaseHTTPRequestHandler, object):

        def __init__(self, *args, **kwargs):
            self.results_handler = results_handler
            self.shutdown_browser = shutdown_browser
            super(MyHandler, self).__init__(*args, **kwargs)

        def do_GET(self):
            # get handler, received request for test settings from web ext runner
            self.send_response(200)
            head, tail = os.path.split(self.path)

            if tail.startswith('raptor') and tail.endswith('.json'):
                LOG.info('reading test settings from ' + tail)
                try:
                    with open(tail) as json_settings:
                        self.send_header('Access-Control-Allow-Origin', '*')
                        self.send_header('Content-type', 'application/json')
                        self.end_headers()
                        self.wfile.write(json.dumps(json.load(json_settings)))
                        self.wfile.close()
                        LOG.info('sent test settings to web ext runner')
                except Exception as ex:
                    LOG.info('control server exception')
                    LOG.info(ex)
            else:
                LOG.info('received request for unknown file: ' + self.path)

        def do_POST(self):
            # post handler, received something from webext
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            content_len = int(self.headers.getheader('content-length'))
            post_body = self.rfile.read(content_len)
            # could have received a status update or test results
            data = json.loads(post_body)
            LOG.info("received " + data['type'] + ": " + str(data['data']))
            if data['type'] == 'webext_results':
                self.results_handler.add(data['data'])
            elif data['data'] == "__raptor_shutdownBrowser":
                # webext is telling us it's done, and time to shutdown the browser
                self.shutdown_browser()

        def do_OPTIONS(self):
            self.send_response(200, "ok")
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            self.send_header("Access-Control-Allow-Headers", "X-Requested-With")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()

    return MyHandler


class RaptorControlServer():
    """Container class for Raptor Control Server"""

    def __init__(self, results_handler):
        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.server = None
        self._server_thread = None
        self.port = None
        self.results_handler = results_handler
        self.browser_proc = None
        self._finished = False

    def start(self):
        config_dir = os.path.join(here, 'tests')
        os.chdir(config_dir)

        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('', 0))
        self.port = sock.getsockname()[1]
        sock.close()
        server_address = ('', self.port)

        server_class = BaseHTTPServer.HTTPServer
        handler_class = MakeCustomHandlerClass(self.results_handler, self.shutdown_browser)

        httpd = server_class(server_address, handler_class)

        self._server_thread = threading.Thread(target=httpd.serve_forever)
        self._server_thread.setDaemon(True)  # don't hang on exit
        self._server_thread.start()
        LOG.info("raptor control server running on port %d..." % self.port)
        self.server = httpd

    def shutdown_browser(self):
        LOG.info("shutting down browser (pid: %d)" % self.browser_proc.pid)
        self.kill_thread = threading.Thread(target=self.wait_for_quit)
        self.kill_thread.daemon = True
        self.kill_thread.start()

    def wait_for_quit(self, timeout=75):
        """Wait timeout seconds for the process to exit. If it hasn't
        exited by then, kill it.
        """
        self.browser_proc.wait(timeout)
        if self.browser_proc.poll() is None:
            self.browser_proc.kill()
        self._finished = True

    def stop(self):
        LOG.info("shutting down control server")
        self.server.shutdown()
        self._server_thread.join()
