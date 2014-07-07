# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from collections import defaultdict
from multiprocessing import current_process
from threading import current_thread, Lock
import time

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


class StructuredLogger(object):
    _lock = Lock()
    _handlers = defaultdict(list)
    """Create a structured logger with the given name

    :param name: The name of the logger.
    """

    def __init__(self, name, component=None):
        self.name = name
        self.component = component

    def add_handler(self, handler):
        """Add a handler to the current logger"""
        self._handlers[self.name].append(handler)

    def remove_handler(self, handler):
        """Remove a handler from the current logger"""
        for i, candidate_handler in enumerate(self._handlers[self.name][:]):
            if candidate_handler == handler:
                del self._handlers[self.name][i]
                break

    @property
    def handlers(self):
        """A list of handlers that will be called when a
        message is logged from this logger"""
        return self._handlers[self.name]

    def _log_data(self, action, data=None):
        if data is None:
            data = {}
        with self._lock:
            log_data = self._make_log_data(action, data)
            for handler in self.handlers:
                handler(log_data)

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

    def suite_start(self, tests, run_info=None):
        """Log a suite_start message

        :param tests: List of test identifiers that will be run in the suite.
        """
        data = {"tests": tests}
        if run_info is not None:
            data["run_info"] = run_info
        self._log_data("suite_start", data)

    def suite_end(self):
        """Log a suite_end message"""
        self._log_data("suite_end")

    def test_start(self, test):
        """Log a test_start message

        :param test: Identifier of the test that will run.
        """
        self._log_data("test_start", {"test": test})

    def test_status(self, test, subtest, status, expected="PASS", message=None):
        """
        Log a test_status message indicating a subtest result. Tests that
        do not have subtests are not expected to produce test_status messages.

        :param test: Identifier of the test that produced the result.
        :param subtest: Name of the subtest.
        :param status: Status string indicating the subtest result
        :param expected: Status string indicating the expected subtest result.
        :param message: String containing a message associated with the result.
        """
        if status.upper() not in ["PASS", "FAIL", "TIMEOUT", "NOTRUN", "ASSERT"]:
            raise ValueError("Unrecognised status %s" % status)
        data = {"test": test,
                "subtest": subtest,
                "status": status.upper()}
        if message is not None:
            data["message"] = message
        if expected != data["status"]:
            data["expected"] = expected
        self._log_data("test_status", data)

    def test_end(self, test, status, expected="OK", message=None, extra=None):
        """
        Log a test_end message indicating that a test completed. For tests
        with subtests this indicates whether the overall test completed without
        errors. For tests without subtests this indicates the test result
        directly.

        :param test: Identifier of the test that produced the result.
        :param status: Status string indicating the test result
        :param expected: Status string indicating the expected test result.
        :param message: String containing a message associated with the result.
        :param extra: suite-specific data associated with the test result.
        """
        if status.upper() not in ["PASS", "FAIL", "OK", "ERROR", "TIMEOUT",
                                  "CRASH", "ASSERT", "SKIP"]:
            raise ValueError("Unrecognised status %s" % status)
        data = {"test": test,
                "status": status.upper()}
        if message is not None:
            data["message"] = message
        if expected != data["status"] and status != "SKIP":
            data["expected"] = expected
        if extra is not None:
            data["extra"] = extra
        self._log_data("test_end", data)

    def process_output(self, process, data, command=None):
        """Log output from a managed process.

        :param process: A unique identifier for the process producing the output
                        (typically the pid)
        :param data: The output to log
        :param command: A string representing the full command line used to start
                        the process.
        """
        data = {"process": process, "data": data}
        if command is not None:
            data["command"] = command
        self._log_data("process_output", data)


def _log_func(level_name):
    def log(self, message):
        data = {"level": level_name, "message": message}
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

