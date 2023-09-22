#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" does_it_crash.py

    Runs a thing to see if it crashes within a set period.
"""
import os
import signal
import subprocess
import sys

import requests

sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozinstall
import mozprocess
from mozharness.base.script import BaseScript


class DoesItCrash(BaseScript):
    config_options = [
        [
            [
                "--thing-url",
            ],
            {
                "action": "store",
                "dest": "thing_url",
                "type": str,
                "help": "An URL that points to a package containing the thing to run",
            },
        ],
        [
            [
                "--thing-to-run",
            ],
            {
                "action": "store",
                "dest": "thing_to_run",
                "type": str,
                "help": "The thing to run. If --thing-url is a package, this should be "
                "its location relative to the root of the package.",
            },
        ],
        [
            [
                "--thing-arg",
            ],
            {
                "action": "append",
                "dest": "thing_args",
                "type": str,
                "default": [],
                "help": "Args for the thing. May be passed multiple times",
            },
        ],
        [
            [
                "--run-for",
            ],
            {
                "action": "store",
                "dest": "run_for",
                "default": 30,
                "type": int,
                "help": "How long to run the thing for, in seconds",
            },
        ],
    ]

    def __init__(self):
        super(DoesItCrash, self).__init__(
            all_actions=[
                "download",
                "run-thing",
            ],
            default_actions=[
                "download",
                "run-thing",
            ],
            config_options=self.config_options,
        )

    def downloadFile(self, url, file_name):
        req = requests.get(url, stream=True, timeout=30)
        file_path = os.path.join(os.getcwd(), file_name)

        with open(file_path, "wb") as f:
            for chunk in req.iter_content(chunk_size=1024):
                if not chunk:
                    continue
                f.write(chunk)
                f.flush()
        return file_path

    def download(self):
        url = self.config["thing_url"]
        fn = "thing." + url.split(".")[-1]
        self.downloadFile(url=url, file_name=fn)
        if mozinstall.is_installer(fn):
            self.install_dir = mozinstall.install(fn, "thing")
        else:
            self.install_dir = ""

    def kill(self, proc):
        is_win = os.name == "nt"
        for retry in range(3):
            if is_win:
                proc.send_signal(signal.CTRL_BREAK_EVENT)
            else:
                os.killpg(proc.pid, signal.SIGKILL)
            try:
                proc.wait(5)
                self.log("process terminated")
                break
            except subprocess.TimeoutExpired:
                self.error("unable to terminate process!")

    def run_thing(self):
        self.timed_out = False

        def timeout_handler(proc):
            self.log(f"timeout detected: killing pid {proc.pid}")
            self.timed_out = True
            self.kill(proc)

        self.output = []

        def output_line_handler(proc, line):
            self.output.append(line)

        thing = os.path.abspath(
            os.path.join(self.install_dir, self.config["thing_to_run"])
        )
        # thing_args is a LockedTuple, which mozprocess doesn't like
        args = list(self.config["thing_args"])
        timeout = self.config["run_for"]

        self.log(f"Running {thing} with args {args}")
        cmd = [thing]
        cmd.extend(args)
        mozprocess.run_and_wait(
            cmd,
            timeout=timeout,
            timeout_handler=timeout_handler,
            output_line_handler=output_line_handler,
        )
        if not self.timed_out:
            # It crashed, oh no!
            self.critical(
                f"TEST-UNEXPECTED-FAIL: {thing} did not run for {timeout} seconds"
            )
            self.critical("Output was:")
            for l in self.output:
                self.critical(l)
            self.fatal("fail")
        else:
            self.info(f"PASS: {thing} ran successfully for {timeout} seconds")


# __main__ {{{1
if __name__ == "__main__":
    crashit = DoesItCrash()
    crashit.run_and_exit()
