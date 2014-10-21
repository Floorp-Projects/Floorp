# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import subprocess
import threading
import urlparse

from mozprocess import ProcessHandler

from .base import testharness_result_converter
from .process import ProcessTestExecutor


class ServoTestharnessExecutor(ProcessTestExecutor):
    convert_result = testharness_result_converter

    def __init__(self, *args, **kwargs):
        ProcessTestExecutor.__init__(self, *args, **kwargs)
        self.result_data = None
        self.result_flag = None

    def run_test(self, test):
        self.result_data = None
        self.result_flag = threading.Event()

        self.command = [self.binary, "--hard-fail",
                        urlparse.urljoin(self.http_server_url, test.url)]

        if self.debug_args:
            self.command = list(self.debug_args) + self.command


        self.proc = ProcessHandler(self.command,
                                   processOutputLine=[self.on_output])
        self.proc.run()

        timeout = test.timeout * self.timeout_multiplier

        # Now wait to get the output we expect, or until we reach the timeout
        self.result_flag.wait(timeout + 5)

        if self.result_flag.is_set():
            assert self.result_data is not None
            self.result_data["test"] = test.url
            result = self.convert_result(test, self.result_data)
            self.proc.kill()
        else:
            if self.proc.pid is None:
                result = (test.result_cls("CRASH", None), [])
            else:
                self.proc.kill()
                result = (test.result_cls("TIMEOUT", None), [])
        self.runner.send_message("test_ended", test, result)

    def on_output(self, line):
        prefix = "ALERT: RESULT: "
        line = line.decode("utf8", "replace")
        if line.startswith(prefix):
            self.result_data = json.loads(line[len(prefix):])
            self.result_flag.set()
        else:
            if self.interactive:
                print line
            else:
                self.logger.process_output(self.proc.pid,
                                           line,
                                           " ".join(self.command))
