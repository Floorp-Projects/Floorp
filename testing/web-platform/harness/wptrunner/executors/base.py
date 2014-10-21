# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
from abc import ABCMeta, abstractmethod

here = os.path.split(__file__)[0]


def executor_kwargs(http_server_url, **kwargs):
    timeout_multiplier = kwargs["timeout_multiplier"]
    if timeout_multiplier is None:
        timeout_multiplier = 1

    executor_kwargs = {"http_server_url": http_server_url,
                       "timeout_multiplier": timeout_multiplier}
    return executor_kwargs


class TestharnessResultConverter(object):
    harness_codes = {0: "OK",
                     1: "ERROR",
                     2: "TIMEOUT"}

    test_codes = {0: "PASS",
                  1: "FAIL",
                  2: "TIMEOUT",
                  3: "NOTRUN"}

    def __call__(self, test, result):
        """Convert a JSON result into a (TestResult, [SubtestResult]) tuple"""
        assert result["test"] == test.url, ("Got results from %s, expected %s" %
                                            (result["test"], test.url))
        harness_result = test.result_cls(self.harness_codes[result["status"]], result["message"])
        return (harness_result,
                [test.subtest_result_cls(subtest["name"], self.test_codes[subtest["status"]],
                                         subtest["message"]) for subtest in result["tests"]])
testharness_result_converter = TestharnessResultConverter()


def reftest_result_converter(self, test, result):
    return (test.result_cls(result["status"], result["message"]), [])


class TestExecutor(object):
    __metaclass__ = ABCMeta

    convert_result = None

    def __init__(self, browser, http_server_url, timeout_multiplier=1):
        """Abstract Base class for object that actually executes the tests in a
        specific browser. Typically there will be a different TestExecutor
        subclass for each test type and method of executing tests.

        :param browser: ExecutorBrowser instance providing properties of the
                        browser that will be tested.
        :param http_server_url: Base url of the http server on which the tests
                                are running.
        :param timeout_multiplier: Multiplier relative to base timeout to use
                                   when setting test timeout.
        """
        self.runner = None
        self.browser = browser
        self.http_server_url = http_server_url
        self.timeout_multiplier = timeout_multiplier

    @property
    def logger(self):
        """StructuredLogger for this executor"""
        if self.runner is not None:
            return self.runner.logger

    @abstractmethod
    def setup(self, runner):
        """Run steps needed before tests can be started e.g. connecting to
        browser instance

        :param runner: TestRunner instance that is going to run the tests"""
        pass

    def teardown(self):
        """Run cleanup steps after tests have finished"""
        pass

    @abstractmethod
    def run_test(self, test):
        """Run a particular test.

        :param test: The test to run"""
        pass
