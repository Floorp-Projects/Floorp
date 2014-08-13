# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from multiprocessing import current_process
from threading import current_thread, Lock
import json
import time

from logtypes import Unicode, TestId, Status, SubStatus, Dict, List, log_action, convertor_registry

"""Structured Logging for recording test results.

Allowed actions, and subfields:
  suite_start
      tests  - List of test names

  suite_end

  test_start
      test - ID for the test

  test_end
      test - ID for the test
      status [PASS | FAIL | OK | ERROR |
              TIMEOUT | CRASH | ASSERT | SKIP] - test status
      expected [As for status] - Status that the test was expected to get,
                                 or absent if the test got the expected status
      extra - Dictionary of harness-specific extra information e.g. debug info

  test_status
      test - ID for the test
      subtest - Name of the subtest
      status [PASS | FAIL | TIMEOUT | NOTRUN] - test status
      expected [As for status] - Status that the subtest was expected to get,
                                 or absent if the subtest got the expected status

  process_output
      process - PID of the process
      command - Command line of the process
      data - Output data from the process

  log
      level [CRITICAL | ERROR | WARNING |
             INFO | DEBUG] - level of the logging message
      message - Message to log

Subfields for all messages:
      action - the action type of the current message
      time - the timestamp in ms since the epoch of the log message
      thread - name for the thread emitting the message
      pid - id of the python process in which the logger is running
      source - name for the source emitting the message
      component - name of the subcomponent emitting the message
"""

_default_logger_name = None

def get_default_logger(component=None):
    """Gets the default logger if available, optionally tagged with component
    name. Will return None if not yet set

    :param component: The component name to tag log messages with
    """
    global _default_logger_name

    if not _default_logger_name:
        return None

    return StructuredLogger(_default_logger_name, component=component)

def set_default_logger(default_logger):
    """Sets the default logger to logger.

    It can then be retrieved with :py:func:`get_default_logger`

    :param default_logger: The logger to set to default.
    """
    global _default_logger_name

    _default_logger_name = default_logger.name

log_levels = dict((k.upper(), v) for v, k in
                  enumerate(["critical", "error", "warning", "info", "debug"]))


class LoggerState(object):
    def __init__(self):
        self.handlers = []
        self.running_tests = set()
        self.suite_started = False

class StructuredLogger(object):
    _lock = Lock()
    _logger_states = {}
    """Create a structured logger with the given name

    :param name: The name of the logger.
    :param component: A subcomponent that the logger belongs to (typically a library name)
    """

    def __init__(self, name, component=None):
        self.name = name
        self.component = component

        with self._lock:
            if name not in self._logger_states:
                self._logger_states[name] = LoggerState()

    @property
    def _state(self):
        return self._logger_states[self.name]

    def add_handler(self, handler):
        """Add a handler to the current logger"""
        self._state.handlers.append(handler)

    def remove_handler(self, handler):
        """Remove a handler from the current logger"""
        for i, candidate_handler in enumerate(self._state.handlers[:]):
            if candidate_handler == handler:
                del self._state.handlers[i]
                break

    @property
    def handlers(self):
        """A list of handlers that will be called when a
        message is logged from this logger"""
        return self._state.handlers

    def log_raw(self, raw_data):
        if "action" not in raw_data:
            raise ValueError

        action = raw_data["action"]
        converted_data = convertor_registry[action].convert_known(**raw_data)
        for k, v in raw_data.iteritems():
            if k not in converted_data:
                converted_data[k] = v

        data = self._make_log_data(action, converted_data)

        if action in ("test_status", "test_end"):
            if (data["expected"] == data["status"] or
                data["status"] == "SKIP" or
                "expected" not in raw_data):
                del data["expected"]

        self._handle_log(data)

    def _log_data(self, action, data=None):
        if data is None:
            data = {}

        log_data = self._make_log_data(action, data)
        self._handle_log(log_data)

    def _handle_log(self, data):
        with self._lock:
            for handler in self.handlers:
                handler(data)

    def _make_log_data(self, action, data):
        all_data = {"action": action,
                    "time": int(time.time() * 1000),
                    "thread": current_thread().name,
                    "pid": current_process().pid,
                    "source": self.name}
        if self.component:
            all_data['component'] = self.component
        all_data.update(data)
        return all_data

    @log_action(List("tests", Unicode),
                Dict("run_info", default=None, optional=True))
    def suite_start(self, data):
        """Log a suite_start message

        :param tests: List of test identifiers that will be run in the suite.
        """
        if self._state.suite_started:
            self.error("Got second suite_start message before suite_end. Logged with data %s" %
                       json.dumps(data))
            return

        self._state.suite_started = True

        self._log_data("suite_start", data)

    @log_action()
    def suite_end(self, data):
        """Log a suite_end message"""
        if not self._state.suite_started:
            self.error("Got suite_end message before suite_start.")
            return

        self._state.suite_started = False

        self._log_data("suite_end")

    @log_action(TestId("test"))
    def test_start(self, data):
        """Log a test_start message

        :param test: Identifier of the test that will run.
        """
        if not self._state.suite_started:
            self.error("Got test_start message before suite_start for test %s" %
                       data["test"])
            return
        if data["test"] in self._state.running_tests:
            self.error("test_start for %s logged while in progress." %
                       data["test"])
            return
        self._state.running_tests.add(data["test"])
        self._log_data("test_start", data)

    @log_action(TestId("test"),
                Unicode("subtest"),
                SubStatus("status"),
                SubStatus("expected", default="PASS"),
                Unicode("message", default=None, optional=True),
                Unicode("stack", default=None, optional=True),
                Dict("extra", default=None, optional=True))
    def test_status(self, data):
        """
        Log a test_status message indicating a subtest result. Tests that
        do not have subtests are not expected to produce test_status messages.

        :param test: Identifier of the test that produced the result.
        :param subtest: Name of the subtest.
        :param status: Status string indicating the subtest result
        :param expected: Status string indicating the expected subtest result.
        :param message: String containing a message associated with the result.
        :param stack: a stack trace encountered during test execution.
        :param extra: suite-specific data associated with the test result.
        """

        if (data["expected"] == data["status"] or
            data["status"] == "SKIP"):
            del data["expected"]

        if data["test"] not in self._state.running_tests:
            self.error("test_status for %s logged while not in progress. "
                       "Logged with data: %s" % (data["test"], json.dumps(data)))
            return

        self._log_data("test_status", data)

    @log_action(TestId("test"),
                Status("status"),
                Status("expected", default="OK"),
                Unicode("message", default=None, optional=True),
                Unicode("stack", default=None, optional=True),
                Dict("extra", default=None, optional=True))
    def test_end(self, data):
        """
        Log a test_end message indicating that a test completed. For tests
        with subtests this indicates whether the overall test completed without
        errors. For tests without subtests this indicates the test result
        directly.

        :param test: Identifier of the test that produced the result.
        :param status: Status string indicating the test result
        :param expected: Status string indicating the expected test result.
        :param message: String containing a message associated with the result.
        :param stack: a stack trace encountered during test execution.
        :param extra: suite-specific data associated with the test result.
        """

        if (data["expected"] == data["status"] or
             data["status"] == "SKIP"):
            del data["expected"]

        if data["test"] not in self._state.running_tests:
            self.error("test_end for %s logged while not in progress. "
                       "Logged with data: %s" % (data["test"], json.dumps(data)))
        else:
            self._state.running_tests.remove(data["test"])
            self._log_data("test_end", data)

    @log_action(Unicode("process"),
                Unicode("data"),
                Unicode("command", default=None, optional=True))
    def process_output(self, data):
        """Log output from a managed process.

        :param process: A unique identifier for the process producing the output
                        (typically the pid)
        :param data: The output to log
        :param command: A string representing the full command line used to start
                        the process.
        """
        self._log_data("process_output", data)


def _log_func(level_name):
    @log_action(Unicode("message"))
    def log(self, data):
        data["level"] = level_name
        self._log_data("log", data)

    log.__doc__ = """Log a message with level %s

:param message: The string message to log
""" % level_name
    log.__name__ = str(level_name).lower()
    return log


# Create all the methods on StructuredLog for debug levels
for level_name in log_levels:
    setattr(StructuredLogger, level_name.lower(), _log_func(level_name))


class StructuredLogFileLike(object):
    """Wrapper for file-like objects to redirect writes to logger
    instead. Each call to `write` becomes a single log entry of type `log`.

    When using this it is important that the callees i.e. the logging
    handlers do not themselves try to write to the wrapped file as this
    will cause infinite recursion.

    :param logger: `StructuredLogger` to which to redirect the file write operations.
    :param level: log level to use for each write.
    :param prefix: String prefix to prepend to each log entry.
    """
    def __init__(self, logger, level="info", prefix=None):
        self.logger = logger
        self.log_func = getattr(self.logger, level)
        self.prefix = prefix

    def write(self, data):
        if data.endswith("\n"):
            data = data[:-1]
        if data.endswith("\r"):
            data = data[:-1]
        if self.prefix is not None:
            data = "%s: %s" % (self.prefix, data)
        self.log_func(data)

    def flush(self):
        pass

