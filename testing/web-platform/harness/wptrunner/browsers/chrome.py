# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from .base import Browser, ExecutorBrowser, require_arg
from .webdriver import ChromedriverLocalServer
from ..executors.executorselenium import SeleniumTestharnessExecutor, required_files


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
    return {"binary": kwargs["binary"],
            "webdriver_binary": kwargs["webdriver_binary"]}


def executor_kwargs(http_server_url, **kwargs):
    from selenium.webdriver import DesiredCapabilities

    timeout_multiplier = kwargs["timeout_multiplier"]
    if timeout_multiplier is None:
        timeout_multiplier = 1
    binary = kwargs["binary"]
    capabilities = dict(DesiredCapabilities.CHROME.items() +
                        {"chromeOptions": {"binary": binary}}.items())

    return {"http_server_url": http_server_url,
            "capabilities": capabilities,
            "timeout_multiplier": timeout_multiplier}


def env_options():
    return {"host": "localhost",
            "bind_hostname": "true",
            "required_files": required_files}


class ChromeBrowser(Browser):
    """Chrome is backed by chromedriver, which is supplied through
    ``browsers.webdriver.ChromedriverLocalServer``."""

    def __init__(self, logger, binary, webdriver_binary="chromedriver"):
        """Creates a new representation of Chrome.  The `binary` argument gives
        the browser binary to use for testing."""
        Browser.__init__(self, logger)
        self.binary = binary
        self.driver = ChromedriverLocalServer(self.logger, binary=webdriver_binary)

    def start(self):
        self.driver.start()

    def stop(self):
        self.driver.stop()

    def pid(self):
        return self.driver.pid

    def is_alive(self):
        # TODO(ato): This only indicates the driver is alive,
        # and doesn't say anything about whether a browser session
        # is active.
        return self.driver.is_alive()

    def cleanup(self):
        self.stop()

    def executor_browser(self):
        return ExecutorBrowser, {"webdriver_url": self.driver.url}
