#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" does_it_crash.py

    Runs a thing to see if it crashes within a set period.
"""
from __future__ import absolute_import
import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

import mozinstall
from mozprocess import ProcessHandler
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

    def download(self):
        url = self.config["thing_url"]
        fn = "thing." + url.split(".")[-1]
        self.download_file(self.config["thing_url"], file_name=fn)
        if mozinstall.is_installer(fn):
            self.install_dir = mozinstall.install(fn, "thing")
        else:
            self.install_dir = ""

    def run_thing(self):
        thing = os.path.abspath(
            os.path.join(self.install_dir, self.config["thing_to_run"])
        )
        # thing_args is a LockedTuple, which mozprocess doesn't like
        args = list(self.config["thing_args"])
        timeout = self.config["run_for"]

        self.log(f"Running {thing} with args {args}")
        p = ProcessHandler(
            thing,
            args=args,
            shell=False,
            storeOutput=True,
            kill_on_timeout=True,
            stream=False,
        )
        p.run(timeout)
        # Wait for the timeout + a grace period (to make sure we don't interrupt
        # process tear down).
        # Without this, this script could potentially hang
        p.wait(timeout + 10)
        if not p.timedOut:
            # It crashed, oh no!
            self.critical(
                f"TEST-UNEXPECTED-FAIL: {thing} did not run for {timeout} seconds"
            )
            self.critical("Output was:")
            for l in p.output:
                self.critical(l)
            self.fatal("fail")
        else:
            self.info(f"PASS: {thing} ran successfully for {timeout} seconds")


# __main__ {{{1
if __name__ == "__main__":
    crashit = DoesItCrash()
    crashit.run_and_exit()
