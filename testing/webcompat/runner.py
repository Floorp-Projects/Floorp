# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import os
import shutil
import tempfile

import pytest


def run(
    logger,
    path,
    browser_binary,
    webdriver_binary,
    environ=None,
    bug=None,
    debug=False,
    interventions=None,
    config=None,
    headless=False,
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
                "--browser-binary",
                browser_binary,
                "--webdriver-binary",
                webdriver_binary,
            ]

            if debug:
                args.append("--pdb")

            if headless:
                args.append("--headless")

            if config:
                args.append("--config")
                args.append(config)

            if interventions == "enabled":
                args.extend(["-m", "with_interventions"])
            elif interventions == "disabled":
                args.extend(["-m", "without_interventions"])
            elif interventions is not None:
                raise ValueError(f"Invalid value for interventions {interventions}")
            else:
                raise ValueError("Must provide interventions argument")

            if bug is not None:
                args.extend(["-k", bug])

            args.append(path)
            try:
                logger.suite_start([], name="webcompat-interventions")
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
            default=4444,
            help="Port on which to run WebDriver",
        )
        parser.addoption(
            "--browser", action="store", choices=["firefox"], help="Name of the browser"
        )
        parser.addoption("--bug", action="store", help="Bug number to run tests for")
        parser.addoption(
            "--config",
            action="store",
            help="Path to JSON file containing logins and other settings",
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
