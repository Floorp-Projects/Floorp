# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import json
import os
import subprocess
import tempfile
import threading
import urlparse
import uuid
from collections import defaultdict

from mozprocess import ProcessHandler

from .base import testharness_result_converter, reftest_result_converter
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

        self.command = [self.binary, "--cpu", "--hard-fail", "-z",
                        urlparse.urljoin(self.http_server_url, test.url)]

        if self.debug_args:
            self.command = list(self.debug_args) + self.command


        self.proc = ProcessHandler(self.command,
                                   processOutputLine=[self.on_output],
                                   onFinish=self.on_finish)
        self.proc.run()

        timeout = test.timeout * self.timeout_multiplier

        # Now wait to get the output we expect, or until we reach the timeout
        self.result_flag.wait(timeout + 5)

        if self.result_flag.is_set() and self.result_data is not None:
            self.result_data["test"] = test.url
            result = self.convert_result(test, self.result_data)
            self.proc.kill()
        else:
            if self.proc.proc.poll() is not None:
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

    def on_finish(self):
        self.result_flag.set()


class TempFilename(object):
    def __init__(self, directory):
        self.directory = directory
        self.path = None

    def __enter__(self):
        self.path = os.path.join(self.directory, str(uuid.uuid4()))
        return self.path

    def __exit__(self, *args, **kwargs):
        try:
            os.unlink(self.path)
        except OSError:
            pass


class ServoReftestExecutor(ProcessTestExecutor):
    convert_result = reftest_result_converter

    def __init__(self, *args, **kwargs):
        ProcessTestExecutor.__init__(self, *args, **kwargs)
        self.ref_hashes = {}
        self.ref_urls_by_hash = defaultdict(set)
        self.tempdir = tempfile.mkdtemp()

    def teardown(self):
        os.rmdir(self.tempdir)
        ProcessTestExecutor.teardown(self)

    def run_test(self, test):
        test_url, ref_type, ref_url = test.url, test.ref_type, test.ref_url
        hashes = {"test": None,
                  "ref": self.ref_hashes.get(ref_url)}

        status = None

        for url_type, url in [("test", test_url), ("ref", ref_url)]:
            if hashes[url_type] is None:
                full_url = urlparse.urljoin(self.http_server_url, url)

                with TempFilename(self.tempdir) as output_path:
                    self.command = [self.binary, "--cpu", "--hard-fail", "--exit",
                                    "--output=%s" % output_path, full_url]

                    timeout = test.timeout * self.timeout_multiplier
                    self.proc = ProcessHandler(self.command,
                                               processOutputLine=[self.on_output])
                    self.proc.run()
                    rv = self.proc.wait(timeout=timeout)

                    if rv is None:
                        status = "EXTERNAL-TIMEOUT"
                        self.proc.kill()
                        break

                    if rv < 0:
                        status = "CRASH"
                        break

                    with open(output_path) as f:
                        # Might need to strip variable headers or something here
                        data = f.read()
                        hashes[url_type] = hashlib.sha1(data).hexdigest()

        if status is None:
            self.ref_urls_by_hash[hashes["ref"]].add(ref_url)
            self.ref_hashes[ref_url] = hashes["ref"]

            if ref_type == "==":
                passed = hashes["test"] == hashes["ref"]
            elif ref_type == "!=":
                passed = hashes["test"] != hashes["ref"]
            else:
                raise ValueError

            status = "PASS" if passed else "FAIL"

        result = self.convert_result(test, {"status": status, "message": None})
        self.runner.send_message("test_ended", test, result)


    def on_output(self, line):
        line = line.decode("utf8", "replace")
        if self.interactive:
            print line
        else:
            self.logger.process_output(self.proc.pid,
                                       line,
                                       " ".join(self.command))
