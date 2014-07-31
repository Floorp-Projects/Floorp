# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

try:
    import blessings
except ImportError:
    blessings = None

import base

def format_seconds(total):
    """Format number of seconds to MM:SS.DD form."""
    minutes, seconds = divmod(total, 60)
    return '%2d:%05.2f' % (minutes, seconds)


class MachFormatter(base.BaseFormatter):
    def __init__(self, start_time=None, write_interval=False, write_times=True,
                 terminal=None, disable_colors=False):

        if disable_colors:
            terminal = None
        elif terminal is None and blessings is not None:
            terminal = blessings.Terminal()

        if start_time is None:
            start_time = time.time()
        start_time = int(start_time * 1000)
        self.start_time = start_time
        self.write_interval = write_interval
        self.write_times = write_times
        self.status_buffer = {}
        self.has_unexpected = {}
        self.last_time = None
        self.terminal = terminal

    def __call__(self, data):
        s = base.BaseFormatter.__call__(self, data)
        if s is None:
            return

        time = format_seconds(self._time(data))
        action = data["action"].upper()
        thread = data["thread"]

        if self.terminal is not None:
            test = self._get_test_id(data)

            time = self.terminal.blue(time)

            color = None

            if data["action"] == "test_end":
                if "expected" not in data and not self.has_unexpected[test]:
                    color = self.terminal.green
                else:
                    color = self.terminal.red
            elif data["action"] in ("suite_start", "suite_end", "test_start"):
                color = self.terminal.yellow

            if color is not None:
                action = color(action)

        return "%s %s: %s %s\n" % (time, action, thread, s)

    def _get_test_id(self, data):
        test_id = data.get("test")
        if isinstance(test_id, list):
            test_id = tuple(test_id)
        return test_id

    def suite_start(self, data):
        return "%i" % len(data["tests"])

    def suite_end(self, data):
        return ""

    def test_start(self, data):
        return "%s" % (self._get_test_id(data),)

    def test_end(self, data):
        subtests = self._get_subtest_data(data)
        unexpected = subtests["unexpected"]
        if "expected" in data:
            parent_unexpected = True
            expected_str = ", expected %s" % data["expected"]
            unexpected.append(("[Parent]", data["status"], data["expected"],
                               data.get("message", "")))
        else:
            parent_unexpected = False
            expected_str = ""

        #Reset the counts to 0
        test = self._get_test_id(data)
        self.status_buffer[test] = {"count": 0, "unexpected": [], "pass": 0}
        self.has_unexpected[test] = bool(unexpected)

        if subtests["count"] != 0:
            rv = "Harness %s%s. Subtests passed %i/%i. Unexpected %s" % (
                data["status"], expected_str, subtests["pass"], subtests["count"],
                len(unexpected))
        else:
            rv = "%s%s" % (data["status"], expected_str)

        if unexpected:
            rv += "\n"
            if len(unexpected) == 1 and parent_unexpected:
                rv += "%s" % unexpected[0][-1]
            else:
                for name, status, expected, message in unexpected:
                    expected_str = "Expected %s, got %s" % (expected, status)
                    rv += "%s\n" % ("\n".join([name, "-" * len(name), expected_str, message]))
                rv = rv[:-1]
        return rv

    def test_status(self, data):
        test = self._get_test_id(data)
        if test not in self.status_buffer:
            self.status_buffer[test] = {"count": 0, "unexpected": [], "pass": 0}
        self.status_buffer[test]["count"] += 1

        if "expected" in data:
            self.status_buffer[test]["unexpected"].append((data["subtest"],
                                                           data["status"],
                                                           data["expected"],
                                                           data.get("message", "")))
        if data["status"] == "PASS":
            self.status_buffer[test]["pass"] += 1

    def process_output(self, data):
        return '"%s" (pid:%s command:%s)' % (data["data"],
                                             data["process"],
                                             data["command"])

    def log(self, data):
        level = data.get("level").upper()

        if self.terminal is not None:
            if level in ("CRITICAL", "ERROR"):
                level = self.terminal.red(level)
            elif level == "WARNING":
                level = self.terminal.yellow(level)
            elif level == "INFO":
                level = self.terminal.blue(level)

        if data.get('component'):
            return " ".join([data["component"], level, data["message"]])

        return "%s %s" % (level, data["message"])

    def _get_subtest_data(self, data):
        test = self._get_test_id(data)
        return self.status_buffer.get(test, {"count": 0, "unexpected": [], "pass": 0})

    def _time(self, data):
        entry_time = data["time"]
        if self.write_interval and self.last_time is not None:
            t = entry_time - self.last_time
            self.last_time = entry_time
        else:
            t = entry_time - self.start_time

        return t / 1000.

