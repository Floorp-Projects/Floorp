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


def MakeCustomHandlerClass(results_handler, shutdown_browser, write_raw_gecko_profile):

    class MyHandler(BaseHTTPServer.BaseHTTPRequestHandler, object):

        def __init__(self, *args, **kwargs):
            self.results_handler = results_handler
            self.shutdown_browser = shutdown_browser
            self.write_raw_gecko_profile = write_raw_gecko_profile
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

            if data['type'] == "webext_gecko_profile":
                # received gecko profiling results
                _test = str(data['data'][0])
                _pagecycle = str(data['data'][1])
                _raw_profile = data['data'][2]
                LOG.info("received gecko profile for test %s pagecycle %s" % (_test, _pagecycle))
                self.write_raw_gecko_profile(_test, _pagecycle, _raw_profile)
            elif data['type'] == 'webext_results':
                LOG.info("received " + data['type'] + ": " + str(data['data']))
                self.results_handler.add(data['data'])
            elif data['type'] == "webext_raptor-page-timeout":
                LOG.info("received " + data['type'] + ": " + str(data['data']))
                # pageload test has timed out; record it as a failure
                self.results_handler.add_page_timeout(str(data['data'][0]),
                                                      str(data['data'][1]),
                                                      dict(data['data'][2]))
            elif data['data'] == "__raptor_shutdownBrowser":
                LOG.info("received " + data['type'] + ": " + str(data['data']))
                # webext is telling us it's done, and time to shutdown the browser
                self.shutdown_browser()
            elif data['type'] == 'webext_screenshot':
                LOG.info("received " + data['type'])
                self.results_handler.add_image(str(data['data'][0]),
                                               str(data['data'][1]),
                                               str(data['data'][2]))
            else:
                LOG.info("received " + data['type'] + ": " + str(data['data']))

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

    def __init__(self, results_handler, debug_mode=False):
        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.server = None
        self._server_thread = None
        self.port = None
        self.results_handler = results_handler
        self.browser_proc = None
        self._finished = False
        self.device = None
        self.app_name = None
        self.gecko_profile_dir = None
        self.debug_mode = debug_mode

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
        handler_class = MakeCustomHandlerClass(self.results_handler,
                                               self.shutdown_browser,
                                               self.write_raw_gecko_profile)

        httpd = server_class(server_address, handler_class)

        self._server_thread = threading.Thread(target=httpd.serve_forever)
        self._server_thread.setDaemon(True)  # don't hang on exit
        self._server_thread.start()
        LOG.info("raptor control server running on port %d..." % self.port)
        self.server = httpd

    def shutdown_browser(self):
        # if debug-mode enabled, leave the browser running - require manual shutdown
        # this way the browser console remains open, so we can review the logs etc.
        if self.debug_mode:
            LOG.info("debug-mode enabled, so NOT shutting down the browser")
            self._finished = True
            return

        if self.device is not None:
            LOG.info("shutting down android app %s" % self.app_name)
        else:
            LOG.info("shutting down browser (pid: %d)" % self.browser_proc.pid)
        self.kill_thread = threading.Thread(target=self.wait_for_quit)
        self.kill_thread.daemon = True
        self.kill_thread.start()

    def write_raw_gecko_profile(self, test, pagecycle, profile):
        profile_file = '%s_pagecycle_%s.profile' % (test, pagecycle)
        profile_path = os.path.join(self.gecko_profile_dir, profile_file)
        LOG.info("writing raw gecko profile to disk: %s" % str(profile_path))

        try:
            with open(profile_path, 'w') as profile_file:
                json.dump(profile, profile_file)
                profile_file.close()
        except Exception:
            LOG.critical("Encountered an exception whie writing raw gecko profile to disk")

    def wait_for_quit(self, timeout=15):
        """Wait timeout seconds for the process to exit. If it hasn't
        exited by then, kill it.
        """
        if self.device is not None:
            self.device.stop_application(self.app_name)
        else:
            self.browser_proc.wait(timeout)
            if self.browser_proc.poll() is None:
                self.browser_proc.kill()
        self._finished = True

    def submit_supporting_data(self, supporting_data):
        '''
        Allow the submission of supporting data i.e. power data.
        This type of data is measured outside of the webext; so
        we can submit directly to the control server instead of
        doing an http post.
        '''
        self.results_handler.add_supporting_data(supporting_data)

    def stop(self):
        LOG.info("shutting down control server")
        self.server.shutdown()
        self._server_thread.join()
