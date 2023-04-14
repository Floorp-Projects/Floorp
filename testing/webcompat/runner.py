# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import os
import shutil
import tempfile

import pytest

GVE = "org.mozilla.geckoview_example"


def run(
    logger,
    path,
    webdriver_binary,
    webdriver_port,
    webdriver_ws_port,
    browser_binary=None,
    device_serial=None,
    package_name=None,
    environ=None,
    bug=None,
    debug=False,
    interventions=None,
    shims=None,
    config=None,
    headless=False,
    addon=None,
    do2fa=False,
):
    """"""
    old_environ = os.environ.copy()
    try:
        with TemporaryDirectory() as cache:
            if environ:
                os.environ.update(environ)

            config_plugin = WDConfig()
            result_recorder = ResultRecorder(logger)

            args = [
                "--strict",  # turn warnings into errors
                "-vv",  # show each individual subtest and full failure logs
                "--capture",
                "no",  # enable stdout/stderr from tests
                "--basetemp",
                cache,  # temporary directory
                "--showlocals",  # display contents of variables in local scope
                "-p",
                "no:mozlog",  # use the WPT result recorder
                "--disable-warnings",
                "-rfEs",
                "-p",
                "no:cacheprovider",  # disable state preservation across invocations
                "-o=console_output_style=classic",  # disable test progress bar
                "--browser",
                "firefox",
                "--webdriver-binary",
                webdriver_binary,
                "--webdriver-port",
                webdriver_port,
                "--webdriver-ws-port",
                webdriver_ws_port,
            ]

            if debug:
                args.append("--pdb")

            if headless:
                args.append("--headless")

            if browser_binary:
                args.append("--browser-binary")
                args.append(browser_binary)

            if device_serial:
                args.append("--device-serial")
                args.append(device_serial)

            if package_name:
                args.append("--package-name")
                args.append(package_name)

            if addon:
                args.append("--addon")
                args.append(addon)

            if bug:
                args.append("--bug")
                args.append(bug)

            if do2fa:
                args.append("--do2fa")

            if config:
                args.append("--config")
                args.append(config)

            if interventions is not None and shims is not None:
                raise ValueError(
                    "Must provide only one of interventions or shims argument"
                )
            elif interventions is None and shims is None:
                raise ValueError(
                    "Must provide either an interventions or shims argument"
                )

            name = "webcompat-interventions"
            if interventions == "enabled":
                args.extend(["-m", "with_interventions"])
            elif interventions == "disabled":
                args.extend(["-m", "without_interventions"])
            elif interventions is not None:
                raise ValueError(f"Invalid value for interventions {interventions}")
            if shims == "enabled":
                args.extend(["-m", "with_shims"])
                name = "smartblock-shims"
            elif shims == "disabled":
                args.extend(["-m", "without_shims"])
                name = "smartblock-shims"
            elif shims is not None:
                raise ValueError(f"Invalid value for shims {shims}")
            else:
                name = "smartblock-shims"

            if bug is not None:
                args.extend(["-k", bug])

            args.append(path)
            try:
                logger.suite_start([], name=name)
                pytest.main(args, plugins=[config_plugin, result_recorder])
            except Exception as e:
                logger.critical(str(e))
            finally:
                logger.suite_end()

    finally:
        os.environ = old_environ


class WDConfig:
    def pytest_addoption(self, parser):
        parser.addoption(
            "--browser-binary", action="store", help="Path to browser binary"
        )
        parser.addoption(
            "--webdriver-binary", action="store", help="Path to webdriver binary"
        )
        parser.addoption(
            "--webdriver-port",
            action="store",
            default="4444",
            help="Port on which to run WebDriver",
        )
        parser.addoption(
            "--webdriver-ws-port",
            action="store",
            default="9222",
            help="Port on which to run WebDriver BiDi websocket",
        )
        parser.addoption(
            "--browser", action="store", choices=["firefox"], help="Name of the browser"
        )
        parser.addoption("--bug", action="store", help="Bug number to run tests for")
        parser.addoption(
            "--do2fa",
            action="store_true",
            default=False,
            help="Do two-factor auth live in supporting tests",
        )
        parser.addoption(
            "--config",
            action="store",
            help="Path to JSON file containing logins and other settings",
        )
        parser.addoption(
            "--addon",
            action="store",
            help="Path to the webcompat addon XPI to use",
        )
        parser.addoption(
            "--device-serial",
            action="store",
            help="Emulator device serial number",
        )
        parser.addoption(
            "--package-name",
            action="store",
            default=GVE,
            help="Android package to run/connect to",
        )
        parser.addoption(
            "--headless", action="store_true", help="Run browser in headless mode"
        )


class ResultRecorder(object):
    def __init__(self, logger):
        self.logger = logger

    def pytest_runtest_logreport(self, report):
        if report.passed and report.when == "call":
            self.record_pass(report)
        elif report.failed:
            if report.when != "call":
                self.record_error(report)
            else:
                self.record_fail(report)
        elif report.skipped:
            self.record_skip(report)

    def record_pass(self, report):
        self.record(report.nodeid, "PASS")

    def record_fail(self, report):
        # pytest outputs the stacktrace followed by an error message prefixed
        # with "E   ", e.g.
        #
        #        def test_example():
        #  >         assert "fuu" in "foobar"
        #  > E       AssertionError: assert 'fuu' in 'foobar'
        message = ""
        for line in report.longreprtext.splitlines():
            if line.startswith("E   "):
                message = line[1:].strip()
                break

        self.record(report.nodeid, "FAIL", message=message, stack=report.longrepr)

    def record_error(self, report):
        # error in setup/teardown
        if report.when != "call":
            message = "%s error" % report.when
        self.record(report.nodeid, "ERROR", message, report.longrepr)

    def record_skip(self, report):
        self.record(report.nodeid, "SKIP")

    def record(self, test, status, message=None, stack=None):
        if stack is not None:
            stack = str(stack)
        self.logger.test_start(test)
        self.logger.test_end(
            test=test, status=status, expected="PASS", message=message, stack=stack
        )


class TemporaryDirectory(object):
    def __enter__(self):
        self.path = tempfile.mkdtemp(prefix="pytest-")
        return self.path

    def __exit__(self, *args):
        try:
            shutil.rmtree(self.path)
        except OSError as e:
            # no such file or directory
            if e.errno != errno.ENOENT:
                raise
