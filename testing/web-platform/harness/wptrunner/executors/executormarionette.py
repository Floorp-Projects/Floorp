# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import os
import socket
import sys
import threading
import time
import traceback
import urlparse
import uuid
from collections import defaultdict

marionette = None

here = os.path.join(os.path.split(__file__)[0])

from .base import TestExecutor, testharness_result_converter, reftest_result_converter
from ..testrunner import Stop

# Extra timeout to use after internal test timeout at which the harness
# should force a timeout
extra_timeout = 5 # seconds

required_files = [("testharness_runner.html", "", False),
                  ("testharnessreport.js", "resources/", True)]


def do_delayed_imports():
    global marionette
    try:
        import marionette
    except ImportError:
        import marionette_driver.marionette as marionette


class MarionetteTestExecutor(TestExecutor):
    def __init__(self,
                 browser,
                 http_server_url,
                 timeout_multiplier=1,
                 debug_args=None,
                 close_after_done=True):
        do_delayed_imports()

        TestExecutor.__init__(self, browser, http_server_url, timeout_multiplier, debug_args)
        self.marionette_port = browser.marionette_port
        self.marionette = None

        self.timer = None
        self.window_id = str(uuid.uuid4())
        self.close_after_done = close_after_done

    def setup(self, runner):
        """Connect to browser via Marionette."""
        self.runner = runner

        self.logger.debug("Connecting to marionette on port %i" % self.marionette_port)
        self.marionette = marionette.Marionette(host='localhost', port=self.marionette_port)
        # XXX Move this timeout somewhere
        self.logger.debug("Waiting for Marionette connection")
        while True:
            success = self.marionette.wait_for_port(60)
            #When running in a debugger wait indefinitely for firefox to start
            if success or self.debug_args is None:
                break

        session_started = False
        if success:
            try:
                self.logger.debug("Starting Marionette session")
                self.marionette.start_session()
            except Exception as e:
                self.logger.warning("Starting marionette session failed: %s" % e)
            else:
                self.logger.debug("Marionette session started")
                session_started = True

        if not success or not session_started:
            self.logger.warning("Failed to connect to Marionette")
            self.runner.send_message("init_failed")
        else:
            try:
                self.after_connect()
            except Exception:
                self.logger.warning("Post-connection steps failed")
                self.logger.error(traceback.format_exc())
                self.runner.send_message("init_failed")
            else:
                self.runner.send_message("init_succeeded")

    def teardown(self):
        try:
            self.marionette.delete_session()
        except:
            # This is typically because the session never started
            pass
        del self.marionette

    def is_alive(self):
        """Check if the marionette connection is still active"""
        try:
            # Get a simple property over the connection
            self.marionette.current_window_handle
        except:
            return False
        return True

    def after_connect(self):
        url = urlparse.urljoin(
            self.http_server_url, "/testharness_runner.html")
        self.logger.debug("Loading %s" % url)
        try:
            self.marionette.navigate(url)
        except:
            self.logger.critical(
                "Loading initial page %s failed. Ensure that the "
                "there are no other programs bound to this port and "
                "that your firewall rules or network setup does not "
                "prevent access." % url)
            raise
        self.marionette.execute_script(
            "document.title = '%s'" % threading.current_thread().name.replace("'", '"'))

    def run_test(self, test):
        """Run a single test.

        This method is independent of the test type, and calls
        do_test to implement the type-sepcific testing functionality.
        """
        # Lock to prevent races between timeouts and other results
        # This might not be strictly necessary if we need to deal
        # with the result changing post-hoc anyway (e.g. due to detecting
        # a crash after we get the data back from marionette)
        result = None
        result_flag = threading.Event()
        result_lock = threading.Lock()

        timeout = test.timeout * self.timeout_multiplier

        def timeout_func():
            with result_lock:
                if not result_flag.is_set():
                    result_flag.set()
                    result = (test.result_cls("EXTERNAL-TIMEOUT", None), [])
                    self.runner.send_message("test_ended", test, result)

        if self.debug_args is None:
            self.timer = threading.Timer(timeout + 2 * extra_timeout, timeout_func)
            self.timer.start()

        try:
            self.marionette.set_script_timeout((timeout + extra_timeout) * 1000)
        except IOError, marionette.errors.InvalidResponseException:
            self.logger.error("Lost marionette connection before starting test")
            return Stop

        try:
            result = self.convert_result(test, self.do_test(test, timeout))
        except marionette.errors.ScriptTimeoutException:
            with result_lock:
                if not result_flag.is_set():
                    result_flag.set()
                    result = (test.result_cls("EXTERNAL-TIMEOUT", None), [])
            # Clean up any unclosed windows
            # This doesn't account for the possibility the browser window
            # is totally hung. That seems less likely since we are still
            # getting data from marionette, but it might be just as well
            # to do a full restart in this case
            # XXX - this doesn't work at the moment because window_handles
            # only returns OS-level windows (see bug 907197)
            # while True:
            #     handles = self.marionette.window_handles
            #     self.marionette.switch_to_window(handles[-1])
            #     if len(handles) > 1:
            #         self.marionette.close()
            #     else:
            #         break
            # Now need to check if the browser is still responsive and restart it if not
        except (socket.timeout, marionette.errors.InvalidResponseException, IOError):
            # This can happen on a crash
            # Also, should check after the test if the firefox process is still running
            # and otherwise ignore any other result and set it to crash
            with result_lock:
                if not result_flag.is_set():
                    result_flag.set()
                    result = (test.result_cls("CRASH", None), [])
        finally:
            if self.timer is not None:
                self.timer.cancel()

        with result_lock:
            if result:
                self.runner.send_message("test_ended", test, result)

    def do_test(self, test, timeout):
        """Run the steps specific to a given test type for Marionette-based tests.

        :param test: - the Test being run
        :param timeout: - the timeout in seconds to give the test
        """
        raise NotImplementedError

class MarionetteTestharnessExecutor(MarionetteTestExecutor):
    convert_result = testharness_result_converter

    def __init__(self, *args, **kwargs):
        """Marionette-based executor for testharness.js tests"""
        MarionetteTestExecutor.__init__(self, *args, **kwargs)
        self.script = open(os.path.join(here, "testharness_marionette.js")).read()

    def do_test(self, test, timeout):
        if self.close_after_done:
            self.marionette.execute_script("if (window.wrappedJSObject.win) {window.wrappedJSObject.win.close()}")

        return self.marionette.execute_async_script(
            self.script % {"abs_url": urlparse.urljoin(self.http_server_url, test.url),
                           "url": test.url,
                           "window_id": self.window_id,
                           "timeout_multiplier": self.timeout_multiplier,
                           "timeout": timeout * 1000,
                           "explicit_timeout": self.debug_args is not None}, new_sandbox=False)


class MarionetteReftestExecutor(MarionetteTestExecutor):
    convert_result = reftest_result_converter

    def __init__(self, *args, **kwargs):
        """Marionette-based executor for reftests"""
        MarionetteTestExecutor.__init__(self, *args, **kwargs)
        with open(os.path.join(here, "reftest.js")) as f:
            self.script = f.read()
        with open(os.path.join(here, "reftest-wait.js")) as f:
            self.wait_script = f.read()
        self.ref_hashes = {}
        self.ref_urls_by_hash = defaultdict(set)

    def do_test(self, test, timeout):
        test_url, ref_type, ref_url = test.url, test.ref_type, test.ref_url
        hashes = {"test": None,
                  "ref": self.ref_hashes.get(ref_url)}
        self.marionette.execute_script(self.script)
        self.marionette.switch_to_window(self.marionette.window_handles[-1])
        for url_type, url in [("test", test_url), ("ref", ref_url)]:
            if hashes[url_type] is None:
                # Would like to do this in a new tab each time, but that isn't
                # easy with the current state of marionette
                full_url = urlparse.urljoin(self.http_server_url, url)
                try:
                    self.marionette.navigate(full_url)
                except marionette.errors.MarionetteException:
                    return {"status": "ERROR",
                            "message": "Failed to load url %s" % (full_url,)}
                if url_type == "test":
                    self.wait()
                screenshot = self.marionette.screenshot()
                # strip off the data:img/png, part of the url
                if screenshot.startswith("data:image/png;base64,"):
                    screenshot = screenshot.split(",", 1)[1]
                hashes[url_type] = hashlib.sha1(screenshot).hexdigest()

        self.ref_urls_by_hash[hashes["ref"]].add(ref_url)
        self.ref_hashes[ref_url] = hashes["ref"]

        if ref_type == "==":
            passed = hashes["test"] == hashes["ref"]
        elif ref_type == "!=":
            passed = hashes["test"] != hashes["ref"]
        else:
            raise ValueError

        return {"status": "PASS" if passed else "FAIL",
                "message": None}

    def wait(self):
        self.marionette.execute_async_script(self.wait_script)

    def teardown(self):
        count = 0
        for hash_val, urls in self.ref_urls_by_hash.iteritems():
            if len(urls) > 1:
                self.logger.info("The following %i reference urls appear to be equivalent:\n %s" %
                                 (len(urls), "\n  ".join(urls)))
                count += len(urls) - 1
        MarionetteTestExecutor.teardown(self)
