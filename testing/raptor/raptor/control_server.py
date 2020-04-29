#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# control server for raptor performance framework
# communicates with the raptor browser webextension
from __future__ import absolute_import

import datetime
import json
import os
import shutil
import six
import socket
import threading
import time

try:
    from http import server  # py3
except ImportError:
    import BaseHTTPServer as server  # py2

from logger.logger import RaptorLogger

LOG = RaptorLogger(component="raptor-control-server")

here = os.path.abspath(os.path.dirname(__file__))


def MakeCustomHandlerClass(
    results_handler,
    error_handler,
    startup_handler,
    shutdown_browser,
    handle_gecko_profile,
    background_app,
    foreground_app
):
    class MyHandler(server.BaseHTTPRequestHandler, object):
        """
        Control server expects messages of the form
        {'type': 'messagetype', 'data':...}

        Each message is given a key which is calculated as

           If type is 'webext_status', then
              the key is data['type']/data['data']
           otherwise
              the key is data['type'].

        The control server can be forced to wait before performing an
        action requested via POST by sending a special message

        {'type': 'wait-set', 'data': key}

        where key is the key of the message for which the control server should
        perform a wait before processing. The handler will store
        this key in the wait_after_messages dict as a True value.

        wait_after_messages[key] = True

        For subsequent requests, the handler will check the key of
        the incoming message against wait_for_messages; if found
        and its value is True, the handler will assign the key
        to waiting_in_state and will loop until the key is removed
        or until its value is changed to False.

        The control server will stop waiting for a state to be continued
        or cleared after wait_timeout seconds, after which the wait
        will be removed and the control server will finish processing
        the current POST request. wait_timeout defaults to 60 seconds
        but can be set globally for all wait states by sending the
        message

        {'type': 'wait-timeout', 'data': timeout}

        The value of waiting_in_state can be retrieved by sending the
        message

        {'type': 'wait-get', 'data': ''}

        which will return the value of waiting_in_state in the
        content of the response. If the value returned is not
        'None', then the control server has received a message whose
        key is recorded in wait_after_messages and is waiting before
        completing the request.

        The control server can be told to stop waiting and finish
        processing the current request while keeping the wait for
        subsequent requests by sending

        {'type': 'wait-continue', 'data': ''}

        The control server can be told to stop waiting and finish
        processing the current request while removing the wait for
        subsequent requests by sending

        {'type': 'wait-clear', 'data': key}

            if key is the empty string ''
                the key in waiting_in_state is removed from wait_after_messages
                waiting_in_state is set to None
            else if key is 'all'
                 all keys in wait_after_messages are removed
            else key is not in wait_after_messages
                 the message is ignored
            else
                 the key is removed from wait_after messages
                 if the key matches the value in waiting_in_state,
                 then waiting_in_state is set to None
        """

        wait_after_messages = {}
        waiting_in_state = None
        wait_timeout = 60

        def __init__(self, *args, **kwargs):
            self.results_handler = results_handler
            self.error_handler = error_handler
            self.startup_handler = startup_handler
            self.shutdown_browser = shutdown_browser
            self.handle_gecko_profile = handle_gecko_profile
            self.background_app = background_app
            self.foreground_app = foreground_app
            super(MyHandler, self).__init__(*args, **kwargs)

        def log_request(self, code="-", size="-"):
            if code != 200:
                super(MyHandler, self).log_request(code, size)

        def do_GET(self):
            # get handler, received request for test settings from webext runner
            self.send_response(200)
            head, tail = os.path.split(self.path)

            if tail.startswith("raptor") and tail.endswith(".json"):
                LOG.info("reading test settings from json/" + tail)
                try:
                    with open("json/{}".format(tail)) as json_settings:
                        self.send_header("Access-Control-Allow-Origin", "*")
                        self.send_header("Content-type", "application/json")
                        self.end_headers()
                        self.wfile.write(json.dumps(json.load(json_settings)))
                        self.wfile.close()
                        LOG.info("sent test settings to webext runner")
                except Exception as ex:
                    LOG.info("control server exception")
                    LOG.info(ex)
            else:
                LOG.info("received request for unknown file: " + self.path)

        def do_POST(self):
            # post handler, received something from webext
            self.send_response(200)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-type", "text/html")
            self.end_headers()

            if six.PY2:
                content_len = int(self.headers.getheader("content-length"))
            elif six.PY3:
                content_len = int(self.headers.get("content-length"))

            post_body = self.rfile.read(content_len)
            # could have received a status update or test results
            if isinstance(post_body, six.binary_type):
                post_body = post_body.decode('utf-8')
            data = json.loads(post_body)

            if data["type"] == "webext_status":
                wait_key = "%s/%s" % (data["type"], data["data"])
            else:
                wait_key = data["type"]

            if MyHandler.wait_after_messages.get(wait_key, None):
                LOG.info("Waiting in %s" % wait_key)
                MyHandler.waiting_in_state = wait_key
                start_time = datetime.datetime.now()

            while MyHandler.wait_after_messages.get(wait_key, None):
                time.sleep(1)
                elapsed_time = datetime.datetime.now() - start_time
                if elapsed_time > datetime.timedelta(seconds=MyHandler.wait_timeout):
                    del MyHandler.wait_after_messages[wait_key]
                    MyHandler.waiting_in_state = None
                    LOG.error(
                        "TEST-UNEXPECTED-ERROR | "
                        "control server wait %s exceeded %s seconds"
                        % (wait_key, MyHandler.wait_timeout)
                    )

            if MyHandler.wait_after_messages.get(wait_key, None) is not None:
                # If the wait is False, it was continued, so we set it back
                # to True for the next time. If removed by clear, we
                # leave it alone so it will not cause further waits.
                MyHandler.wait_after_messages[wait_key] = True

            if data["type"] == "webext_error":
                error, stack = data["data"]
                LOG.info("received " + data["type"] + ": " + str(error))
                self.error_handler(error, stack)

            elif data["type"] == "webext_gecko_profile":
                # received file name of the saved gecko profile
                filename = str(data["data"])
                LOG.info("received gecko profile filename: {}".format(filename))
                self.handle_gecko_profile(filename)

            elif data["type"] == "webext_results":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
                self.results_handler.add(data["data"])
            elif data["type"] == "webext_raptor-page-timeout":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))

                if len(data["data"]) == 3:
                    data["data"].append("")
                # pageload test has timed out; record it as a failure
                self.results_handler.add_page_timeout(
                    str(data["data"][0]), str(data["data"][1]), str(data["data"][2]),
                    dict(data["data"][3])
                )
            elif data["type"] == "webext_shutdownBrowser":
                LOG.info("received request to shutdown the browser")
                self.shutdown_browser()
            elif data["type"] == "webext_start_background":
                LOG.info("received request to background app")
                self.background_app()
            elif data["type"] == "webext_end_background":
                LOG.info("received request to foreground app")
                self.foreground_app()
            elif data["type"] == "webext_screenshot":
                LOG.info("received " + data["type"])
                self.results_handler.add_image(
                    str(data["data"][0]), str(data["data"][1]), str(data["data"][2])
                )
            elif data["type"] == "webext_status":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
            elif data["type"] == "webext_loaded":
                LOG.info("received " + data["type"] + ": raptor runner.js is loaded!")
                self.startup_handler(True)
            elif data["type"] == "wait-set":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
                MyHandler.wait_after_messages[str(data["data"])] = True
            elif data["type"] == "wait-timeout":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
                MyHandler.wait_timeout = data["data"]
            elif data["type"] == "wait-get":
                state = MyHandler.waiting_in_state
                if state is None:
                    state = 'None'
                if isinstance(state, six.text_type):
                    state = state.encode('utf-8')
                self.wfile.write(state)
            elif data["type"] == "wait-continue":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
                if MyHandler.waiting_in_state:
                    MyHandler.wait_after_messages[MyHandler.waiting_in_state] = False
                    MyHandler.waiting_in_state = None
            elif data["type"] == "wait-clear":
                LOG.info("received " + data["type"] + ": " + str(data["data"]))
                clear_key = str(data["data"])
                if clear_key == "":
                    if MyHandler.waiting_in_state:
                        del MyHandler.wait_after_messages[MyHandler.waiting_in_state]
                        MyHandler.waiting_in_state = None
                    else:
                        pass
                elif clear_key == "all":
                    MyHandler.wait_after_messages = {}
                    MyHandler.waiting_in_state = None
                elif clear_key not in MyHandler.wait_after_messages:
                    pass
                else:
                    del MyHandler.wait_after_messages[clear_key]
                    if MyHandler.waiting_in_state == clear_key:
                        MyHandler.waiting_in_state = None
            else:
                LOG.info("received " + data["type"] + ": " + str(data["data"]))

        def do_OPTIONS(self):
            self.send_response(200, "ok")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "X-Requested-With")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()

    return MyHandler


class ThreadedHTTPServer(server.HTTPServer):
    # See
    # https://stackoverflow.com/questions/19537132/threaded-basehttpserver-one-thread-per-request#30312766
    def process_request(self, request, client_address):
        thread = threading.Thread(
            target=self.__new_request,
            args=(self.RequestHandlerClass, request, client_address, self),
        )
        thread.start()

    def __new_request(self, handlerClass, request, address, server):
        handlerClass(request, address, server)
        self.shutdown_request(request)


class RaptorControlServer:
    """Container class for Raptor Control Server"""

    def __init__(self, results_handler, debug_mode=False):
        self.raptor_venv = os.path.join(os.getcwd(), "raptor-venv")
        self.server = None
        self._server_thread = None
        self.port = None
        self.results_handler = results_handler
        self.browser_proc = None
        self._finished = False
        self._is_shutting_down = False
        self._runtime_error = None
        self.device = None
        self.app_name = None
        self.gecko_profile_dir = None
        self.debug_mode = debug_mode
        self.user_profile = None
        self.is_webextension_loaded = False

    def start(self):
        config_dir = os.path.join(here, "tests")
        os.chdir(config_dir)

        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(("", 0))
        self.port = sock.getsockname()[1]
        sock.close()
        server_address = ("", self.port)

        server_class = ThreadedHTTPServer
        handler_class = MakeCustomHandlerClass(
            self.results_handler,
            self.error_handler,
            self.startup_handler,
            self.shutdown_browser,
            self.handle_gecko_profile,
            self.background_app,
            self.foreground_app
        )

        httpd = server_class(server_address, handler_class)

        self._server_thread = threading.Thread(target=httpd.serve_forever)
        self._server_thread.setDaemon(True)  # don't hang on exit
        self._server_thread.start()
        LOG.info("raptor control server running on port %d..." % self.port)
        self.server = httpd

    def error_handler(self, error, stack):
        self._runtime_error = {"error": error, "stack": stack}

    def startup_handler(self, value):
        self.is_webextension_loaded = value

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
        self.kill_thread = threading.Thread(target=self.wait_for_quit, kwargs={"timeout": 0})
        self.kill_thread.daemon = True
        self.kill_thread.start()

    def handle_gecko_profile(self, filename):
        # Move the stored profile to a location outside the Firefox profile
        source_path = os.path.join(self.user_profile.profile, "profiler", filename)
        target_path = os.path.join(self.gecko_profile_dir, filename)
        shutil.move(source_path, target_path)
        LOG.info("moved gecko profile to {}".format(target_path))

    def is_app_in_background(self):
        # Get the app view state: foreground->False, background->True
        current_focus = self.device.shell_output(
            "dumpsys window windows | grep mCurrentFocus"
        ).strip()
        return self.app_name not in current_focus

    def background_app(self):
        # Disable Doze, background the app, then disable App Standby
        self.device.shell_output("dumpsys deviceidle whitelist +%s" % self.app_name)
        self.device.shell_output("input keyevent 3")
        if not self.is_app_in_background():
            LOG.critical("%s is still in foreground after background request" % self.app_name)
        else:
            LOG.info("%s was successfully backgrounded" % self.app_name)

    def foreground_app(self):
        self.device.shell_output("am start --activity-single-top %s" % self.app_name)
        self.device.shell_output("dumpsys deviceidle enable")
        if self.is_app_in_background():
            LOG.critical("%s is still in background after foreground request" % self.app_name)
        else:
            LOG.info("%s was successfully foregrounded" % self.app_name)

    def wait_for_quit(self, timeout=15):
        """Wait timeout seconds for the process to exit. If it hasn't
        exited by then, kill it.

        The sleep calls are required to give those new values enough time
        to sync-up between threads. It would be better to maybe use signals
        for synchronization (bug 1633975)
        """
        self._is_shutting_down = True
        time.sleep(.25)

        if self.device is not None:
            self.device.stop_application(self.app_name)
        else:
            self.browser_proc.wait(timeout)
            if self.browser_proc.poll() is None:
                self.browser_proc.kill()

        self._finished = True
        time.sleep(.25)
        self._is_shutting_down = False

    def submit_supporting_data(self, supporting_data):
        """
        Allow the submission of supporting data i.e. power data.
        This type of data is measured outside of the webext; so
        we can submit directly to the control server instead of
        doing an http post.
        """
        self.results_handler.add_supporting_data(supporting_data)

    def stop(self):
        LOG.info("shutting down control server")
        self.server.shutdown()
        self._server_thread.join()
