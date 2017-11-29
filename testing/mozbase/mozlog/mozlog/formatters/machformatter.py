# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import

import time

try:
    import blessings
except ImportError:
    blessings = None

from . import base
from .process import strstatus
from ..handlers import SummaryHandler


def format_seconds(total):
    """Format number of seconds to MM:SS.DD form."""
    minutes, seconds = divmod(total, 60)
    return '%2d:%05.2f' % (minutes, seconds)


class NullTerminal(object):

    def __getattr__(self, name):
        return self._id

    def _id(self, value):
        return value


class MachFormatter(base.BaseFormatter):

    def __init__(self, start_time=None, write_interval=False, write_times=True,
                 terminal=None, disable_colors=False, **kwargs):
        super(MachFormatter, self).__init__(**kwargs)

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
        self.verbose = False
        self._known_pids = set()

        self.summary = SummaryHandler()

    def __call__(self, data):
        self.summary(data)

        s = super(MachFormatter, self).__call__(data)
        if s is None:
            return

        time = format_seconds(self._time(data))
        action = data["action"].upper()
        thread = data["thread"]

        # Not using the NullTerminal here is a small optimisation to cut the number of
        # function calls
        if self.terminal is not None:
            test = self._get_test_id(data)

            time = self.terminal.blue(time)

            color = None

            if data["action"] == "test_end":
                if "expected" not in data and not self.has_unexpected[test]:
                    color = self.terminal.green
                else:
                    color = self.terminal.red
            elif data["action"] in ("suite_start", "suite_end",
                                    "test_start", "test_status"):
                color = self.terminal.yellow
            elif data["action"] == "crash":
                color = self.terminal.red
            elif data["action"] == "assertion_count":
                if (data["count"] > data["max_expected"] or
                    data["count"] < data["min_expected"]):
                    color = self.terminal.red

            if color is not None:
                action = color(action)

        return "%s %s: %s %s\n" % (time, action, thread, s)

    def _get_test_id(self, data):
        test_id = data.get("test")
        if isinstance(test_id, list):
            test_id = tuple(test_id)
        return test_id

    def _get_file_name(self, test_id):
        if isinstance(test_id, (str, unicode)):
            return test_id

        if isinstance(test_id, tuple):
            return "".join(test_id)

        assert False, "unexpected test_id"

    def suite_start(self, data):
        num_tests = reduce(lambda x, y: x + len(y), data['tests'].itervalues(), 0)
        return "%i" % num_tests

    def suite_end(self, data):
        return self._format_suite_summary(self.summary.current_suite, self.summary.current)

    def _format_expected(self, status, expected):
        term = self.terminal if self.terminal is not None else NullTerminal()
        if status == "ERROR":
            color = term.red
        else:
            color = term.yellow

        if expected in ("PASS", "OK"):
            return color(status)

        return color("%s expected %s" % (status, expected))

    def _format_suite_summary(self, suite, summary):
        term = self.terminal if self.terminal is not None else NullTerminal()

        count = summary['counts']
        logs = summary['unexpected_logs']

        rv = ["", suite, "~" * len(suite)]

        # Format check counts
        checks = self.summary.aggregate('count', count)
        rv.append("Ran {} checks ({})".format(sum(checks.values()),
                  ', '.join(['{} {}s'.format(v, k) for k, v in checks.items() if v])))

        # Format expected counts
        checks = self.summary.aggregate('expected', count, include_skip=False)
        rv.append("Expected results: {}".format(sum(checks.values())))

        # Format skip counts
        skip_tests = count["test"]["expected"]["skip"]
        skip_subtests = count["subtest"]["expected"]["skip"]
        if skip_tests:
            skipped = "Skipped: {} tests".format(skip_tests)
            if skip_subtests:
                skipped = "{}, {} subtests".format(skipped, skip_subtests)
            rv.append(skipped)

        # Format unexpected counts
        checks = self.summary.aggregate('unexpected', count)
        unexpected_count = sum(checks.values())
        if unexpected_count:
            rv.append("Unexpected results: {}".format(unexpected_count))
            for key in ('test', 'subtest', 'assert'):
                if not count[key]['unexpected']:
                    continue
                status_str = ", ".join(["{} {}".format(n, s)
                                        for s, n in count[key]['unexpected'].items()])
                rv.append("  {}: {} ({})".format(
                          key, sum(count[key]['unexpected'].values()), status_str))

        # Format status
        if not any(count[key]["unexpected"] for key in ('test', 'subtest', 'assert')):
            rv.append(term.green("OK"))
        else:
            heading = "Unexpected Logs"
            rv.extend(["", heading, "-" * len(heading)])
            if count['subtest']['count']:
                for test_id, results in logs.items():
                    test = self._get_file_name(test_id)
                    rv.append(test)
                    for data in results:
                        name = data.get("subtest", "[Parent]")
                        rv.append("  %s %s" % (self._format_expected(
                                             data["status"], data["expected"]), name))
            else:
                for test_id, results in logs.items():
                    test = self._get_file_name(test_id)
                    rv.append(test)
                    assert len(results) == 1
                    data = results[0]
                    assert "subtest" not in data
                    rv.append("  %s %s" % (self._format_expected(
                                         data["status"], data["expected"]), test))

        return "\n".join(rv)

    def test_start(self, data):
        return "%s" % (self._get_test_id(data),)

    def test_end(self, data):
        subtests = self._get_subtest_data(data)

        message = data.get("message", "")
        if "stack" in data:
            stack = data["stack"]
            if stack and stack[-1] != "\n":
                stack += "\n"
            message = stack + message

        if "expected" in data:
            parent_unexpected = True
            expected_str = ", expected %s" % data["expected"]
        else:
            parent_unexpected = False
            expected_str = ""

        test = self._get_test_id(data)

        # Reset the counts to 0
        self.status_buffer[test] = {"count": 0, "unexpected": 0, "pass": 0}
        self.has_unexpected[test] = bool(subtests['unexpected'])

        if subtests["count"] != 0:
            rv = "Test %s%s. Subtests passed %i/%i. Unexpected %s" % (
                data["status"], expected_str, subtests["pass"], subtests["count"],
                subtests['unexpected'])
        else:
            rv = "%s%s" % (data["status"], expected_str)

        unexpected = self.summary.current["unexpected_logs"].get(data["test"])
        if unexpected:
            rv += "\n"
            if len(unexpected) == 1 and parent_unexpected:
                rv += "%s" % unexpected[0].get("message", "")
            else:
                for data in unexpected:
                    name = data.get("subtest", "[Parent]")
                    expected_str = "Expected %s, got %s" % (data["expected"], data["status"])
                    rv += "%s\n" % (
                        "\n".join([name, "-" * len(name), expected_str, data.get("message", "")]))
                rv = rv[:-1]
        return rv

    def valgrind_error(self, data):
        rv = " " + data['primary'] + "\n"
        for line in data['secondary']:
            rv = rv + line + "\n"

        return rv

    def test_status(self, data):
        test = self._get_test_id(data)
        if test not in self.status_buffer:
            self.status_buffer[test] = {"count": 0, "unexpected": 0, "pass": 0}
        self.status_buffer[test]["count"] += 1

        message = data.get("message", "")
        if "stack" in data:
            if message:
                message += "\n"
            message += data["stack"]

        if data["status"] == "PASS":
            self.status_buffer[test]["pass"] += 1

        rv = None
        status, subtest = data["status"], data["subtest"]
        unexpected = "expected" in data
        if self.verbose:
            if self.terminal is not None:
                status = (self.terminal.red if unexpected else self.terminal.green)(status)
            rv = " ".join([subtest, status, message])

        if unexpected:
            self.status_buffer[test]["unexpected"] += 1
        return rv

    def assertion_count(self, data):
        if data["min_expected"] <= data["count"] <= data["max_expected"]:
            return

        if data["min_expected"] != data["max_expected"]:
            expected = "%i to %i" % (data["min_expected"],
                                     data["max_expected"])
        else:
            expected = "%i" % data["min_expected"]

        return "Assertion count %i, expected %s assertions\n" % (data["count"], expected)

    def process_output(self, data):
        rv = []

        pid = data['process']
        if pid.isdigit():
            pid = 'pid:%s' % pid

        if "command" in data and data["process"] not in self._known_pids:
            self._known_pids.add(data["process"])
            rv.append('(%s) Full command: %s' % (pid, data["command"]))

        rv.append('(%s) "%s"' % (pid, data["data"]))
        return "\n".join(rv)

    def crash(self, data):
        test = self._get_test_id(data)

        if data.get("stackwalk_returncode", 0) != 0 and not data.get("stackwalk_stderr"):
            success = True
        else:
            success = False

        rv = ["pid:%s. Test:%s. Minidump anaylsed:%s. Signature:[%s]" %
              (data.get("pid", None), test, success, data["signature"])]

        if data.get("minidump_path"):
            rv.append("Crash dump filename: %s" % data["minidump_path"])

        if data.get("stackwalk_returncode", 0) != 0:
            rv.append("minidump_stackwalk exited with return code %d" %
                      data["stackwalk_returncode"])

        if data.get("stackwalk_stderr"):
            rv.append("stderr from minidump_stackwalk:")
            rv.append(data["stackwalk_stderr"])
        elif data.get("stackwalk_stdout"):
            rv.append(data["stackwalk_stdout"])

        if data.get("stackwalk_errors"):
            rv.extend(data.get("stackwalk_errors"))

        rv = "\n".join(rv)
        if not rv[-1] == "\n":
            rv += "\n"

        return rv

    def process_start(self, data):
        rv = "Started process `%s`" % data['process']
        desc = data.get('command')
        if desc:
            rv = '%s (%s)' % (rv, desc)
        return rv

    def process_exit(self, data):
        return "%s: %s" % (data['process'], strstatus(data['exitcode']))

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
            rv = " ".join([data["component"], level, data["message"]])
        else:
            rv = "%s %s" % (level, data["message"])

        if "stack" in data:
            rv += "\n%s" % data["stack"]

        return rv

    def lint(self, data):
        term = self.terminal if self.terminal is not None else NullTerminal()
        fmt = "{path}  {c1}{lineno}{column}  {c2}{level}{normal}  {message}" \
              "  {c1}{rule}({linter}){normal}"
        message = fmt.format(
            path=data["path"],
            normal=term.normal,
            c1=term.grey,
            c2=term.red if data["level"] == 'error' else term.yellow,
            lineno=str(data["lineno"]),
            column=(":" + str(data["column"])) if data.get("column") else "",
            level=data["level"],
            message=data["message"],
            rule='{} '.format(data["rule"]) if data.get("rule") else "",
            linter=data["linter"].lower() if data.get("linter") else "",
        )

        return message

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
