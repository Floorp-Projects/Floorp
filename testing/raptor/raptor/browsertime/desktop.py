#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from logger.logger import RaptorLogger
from perftest import PerftestDesktop

from .base import Browsertime

LOG = RaptorLogger(component="raptor-browsertime-desktop")


class BrowsertimeDesktop(PerftestDesktop, Browsertime):
    def __init__(self, *args, **kwargs):
        super(BrowsertimeDesktop, self).__init__(*args, **kwargs)

    @property
    def browsertime_args(self):
        binary_path = self.config["binary"]
        LOG.info("binary_path: {}".format(binary_path))

        if self.config["app"] == "chrome":
            return ["--browser", self.config["app"], "--chrome.binaryPath", binary_path]
        return ["--browser", self.config["app"], "--firefox.binaryPath", binary_path]

    def setup_chrome_args(self, test):
        # Setup required chrome arguments
        chrome_args = self.desktop_chrome_args(test)

        # Add this argument here, it's added by mozrunner
        # for raptor
        chrome_args.append("--no-first-run")

        btime_chrome_args = []
        for arg in chrome_args:
            btime_chrome_args.extend(["--chrome.args=" + str(arg.replace("'", '"'))])

        return btime_chrome_args
