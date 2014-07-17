# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

import base

def format_seconds(total):
    """Format number of seconds to MM:SS.DD form."""
    minutes, seconds = divmod(total, 60)
    return '%2d:%05.2f' % (minutes, seconds)


class BaseMachFormatter(base.BaseFormatter):
    def __init__(self, start_time=None, write_interval=False, write_times=True):
        if start_time is None:
            start_time = time.time()
        start_time = int(start_time * 1000)
        self.start_time = start_time
        self.write_interval = write_interval
        self.write_times = write_times
        self.status_buffer = {}
        self.has_unexpected = {}
        self.last_time = None

    def __call__(self, data):
        s = base.BaseFormatter.__call__(self, data)
        if s is not None:
            return "%s %s\n" % (self.generic_formatter(data), s)

    def _get_test_id(self, data):
        test_id = data.get("test")
        if isinstance(test_id, list):
            test_id = tuple(test_id)
        return test_id

    def generic_formatter(self, data):
        return "%s: %s" % (data["action"].upper(), data["thread"])

    def suite_start(self, data):
        return "%i" % len(data["tests"])

    def suite_end(self, data):
        return ""

    def test_start(self, data):
        return "%s" % (self._get_test_id(data),)

    def test_end(self, data):
        if "expected" in data:
            expected_str = ", expected %s" % data["expected"]
        else:
            expected_str = ""

        subtests = self._get_subtest_data(data)
        unexpected = subtests["unexpected"] + (1 if "expected" in data else 0)

        #Reset the counts to 0
        test = self._get_test_id(data)
        self.status_buffer[test] = {"count": 0, "unexpected": 0, "pass": 0}
        self.has_unexpected[test] = bool(unexpected)

        return "Harness status %s%s. Subtests passed %i/%i. Unexpected %i" % (
            data["status"], expected_str, subtests["pass"],
            subtests["count"], unexpected)

    def test_status(self, data):
        test = self._get_test_id(data)
        if test not in self.status_buffer:
            self.status_buffer[test] = {"count": 0, "unexpected": 0, "pass": 0}
        self.status_buffer[test]["count"] += 1

        if "expected" in data:
            self.status_buffer[test]["unexpected"] += 1
        if data["status"] == "PASS":
            self.status_buffer[test]["pass"] += 1

    def process_output(self, data):
        return '"%s" (pid:%s command:%s)' % (data["data"],
                                             data["process"],
                                             data["command"])

    def log(self, data):
        if data.get('component'):
            return " ".join([data["component"], data["level"], data["message"]])

        return "%s %s" % (data["level"], data["message"])

    def _get_subtest_data(self, data):
        test = self._get_test_id(data)
        return self.status_buffer.get(test, {"count": 0, "unexpected": 0, "pass": 0})

    def _time(self, data):
        entry_time = data["time"]
        if self.write_interval and self.last_time is not None:
            t = entry_time - self.last_time
            self.last_time = entry_time
        else:
            t = entry_time - self.start_time

        return t / 1000.


class MachFormatter(BaseMachFormatter):
    """Formatter designed for producing human-redable output
    when tests are running. This is principally intended for integration with
    the ``mach`` command dispatch framework.

    :param start_time: time.time() at which the testrun started
    :param write_interval: bool indicating whether to include the interval since the
                           last message
    :param write_times: bool indicating whether to include the time since the testrun
                        started.
    """
    def __call__(self, data):
        s = BaseMachFormatter.__call__(self, data)
        if s is not None:
            return "%s %s" % (format_seconds(self._time(data)), s)


class MachTerminalFormatter(BaseMachFormatter):
    """Formatter designed to produce coloured output in a terminal when tests are
    running. This is principally intended for integration with
    the ``mach`` command dispatch framework.

    :param start_time: time.time() at which the testrun started
    :param write_interval: bool indicating whether to include the interval since the
                           last message
    :param write_times: bool indicating whether to include the time since the testrun
                        started.
    :param terminal: Terminal object from mach.
    """
    def __init__(self, start_time=None, write_interval=False, write_times=True,
                 terminal=None):
        self.terminal = terminal
        BaseMachFormatter.__init__(self,
                                   start_time=start_time,
                                   write_interval=write_interval,
                                   write_times=write_times)

    def __call__(self, data):
        s = BaseMachFormatter.__call__(self, data)
        if s is not None:
            t = self.terminal.blue(format_seconds(self._time(data)))

            return '%s %s' % (t, self._colorize(data, s))

    def _colorize(self, data, s):
        if self.terminal is None:
            return s

        test = self._get_test_id(data)

        color = None
        len_action = len(data["action"])

        if data["action"] == "test_end":
            if "expected" not in data and not self.has_unexpected[test]:
                color = self.terminal.green
            else:
                color = self.terminal.red
        elif data["action"] in ("suite_start", "suite_end", "test_start"):
            color = self.terminal.yellow

        if color is not None:
            result = color(s[:len_action]) + s[len_action:]
        else:
            result = s

        return result
