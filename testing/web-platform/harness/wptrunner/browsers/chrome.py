# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozprocess

from .base import get_free_port, Browser, ExecutorBrowser, require_arg, cmd_arg
from ..executors.executorselenium import SeleniumTestharnessExecutor, required_files


here = os.path.split(__file__)[0]

__wptrunner__ = {"product": "chrome",
                 "check_args": "check_args",
                 "browser": "ChromeBrowser",
                 "executor": {"testharness": "SeleniumTestharnessExecutor"},
                 "browser_kwargs": "browser_kwargs",
                 "executor_kwargs": "executor_kwargs",
                 "env_options": "env_options"}


def check_args(**kwargs):
    require_arg(kwargs, "binary")


def browser_kwargs(**kwargs):
    return {"binary": kwargs["binary"]}


def executor_kwargs(http_server_url, **kwargs):
    from selenium import webdriver

    timeout_multiplier = kwargs["timeout_multiplier"]
    if timeout_multiplier is None:
        timeout_multiplier = 1

    return {"http_server_url": http_server_url,
            "timeout_multiplier": timeout_multiplier,
            "capabilities": webdriver.DesiredCapabilities.CHROME}


def env_options():
    return {"host": "localhost",
            "bind_hostname": "true",
            "required_files": required_files}


class ChromeBrowser(Browser):
    used_ports = set()

    def __init__(self, logger, binary):
        Browser.__init__(self, logger)
        self.binary = binary
        self.webdriver_port = get_free_port(4444, exclude=self.used_ports)
        self.used_ports.add(self.webdriver_port)
        self.proc = None
        self.cmd = None

    def start(self):
        self.cmd = [self.binary,
                    cmd_arg("port", str(self.webdriver_port)),
                    cmd_arg("url-base", "wd/url")]
        self.proc = mozprocess.ProcessHandler(self.cmd, processOutputLine=self.on_output)
        self.logger.debug("Starting chromedriver")
        self.proc.run()

    def stop(self):
        if self.proc is not None and hasattr(self.proc, "proc"):
            self.proc.kill()

    def pid(self):
        if self.proc is not None:
            return self.proc.pid

    def on_output(self, line):
        self.logger.process_output(self.pid(),
                                   line.decode("utf8", "replace"),
                                   command=" ".join(self.cmd))

    def is_alive(self):
        return self.pid() is not None

    def cleanup(self):
        self.stop()

    def executor_browser(self):
        return ExecutorBrowser, {"webdriver_port": self.webdriver_port}
