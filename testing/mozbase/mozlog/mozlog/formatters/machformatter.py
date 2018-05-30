# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import

import time

from mozterm import Terminal

from . import base
from .process import strstatus
from ..handlers import SummaryHandler


def format_seconds(total):
    """Format number of seconds to MM:SS.DD form."""
    minutes, seconds = divmod(total, 60)
    return '%2d:%05.2f' % (minutes, seconds)


class MachFormatter(base.BaseFormatter):

    def __init__(self, start_time=None, write_interval=False, write_times=True,
                 terminal=None, disable_colors=False, summary_on_shutdown=False,
                 verbose=False, **kwargs):
        super(MachFormatter, self).__init__(**kwargs)

        if start_time is None:
            start_time = time.time()
        start_time = int(start_time * 1000)
        self.start_time = start_time
        self.write_interval = write_interval
        self.write_times = write_times
        self.status_buffer = {}
        self.has_unexpected = {}
        self.last_time = None
        self.term = Terminal(disable_styling=disable_colors)
        self.verbose = verbose
        self._known_pids = set()

        self.summary = SummaryHandler()
        self.summary_on_shutdown = summary_on_shutdown

    def __call__(self, data):
        self.summary(data)

        s = super(MachFormatter, self).__call__(data)
        if s is None:
            return

        time = self.term.dim_blue(format_seconds(self._time(data)))
        return "%s %s\n" % (time, s)

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
        action = self.term.yellow(data['action'].upper())
        name = ""
        if 'name' in data:
            name = " %s -" % (data['name'],)
        return "%s:%s running %i tests" % (action, name, num_tests)

    def suite_end(self, data):
        action = self.term.yellow(data['action'].upper())
        rv = [action]
        if not self.summary_on_shutdown:
            rv.append(self._format_suite_summary(self.summary.current_suite, self.summary.current))
        return "\n".join(rv)

    def _format_expected(self, status, expected):
        if status == expected:
            color = self.term.green
            if expected not in ("PASS", "OK"):
                color = self.term.yellow
                status = "EXPECTED-%s" % status
        else:
            color = self.term.red
            if status in ("PASS", "OK"):
                status = "UNEXPECTED-%s" % status
        return color(status)

    def _format_status(self, test, data):
        name = data.get("subtest", test)
        rv = "%s %s" % (self._format_expected(
            data["status"], data.get("expected", data["status"])), name)
        if "message" in data:
            rv += " - %s" % data["message"]
        if "stack" in data:
            rv += self._format_stack(data["stack"])
        return rv

    def _format_stack(self, stack):
        return "\n%s\n" % self.term.dim(stack.strip("\n"))

    def _format_suite_summary(self, suite, summary):
        count = summary['counts']
        logs = summary['unexpected_logs']

        rv = ["", self.term.yellow(suite), self.term.yellow("~" * len(suite))]

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
            rv.append(self.term.green("OK"))
        else:
            heading = "Unexpected Results"
            rv.extend(["", self.term.yellow(heading), self.term.yellow("-" * len(heading))])
            if count['subtest']['count']:
                for test_id, results in logs.items():
                    test = self._get_file_name(test_id)
                    rv.append(self.term.bold(test))
                    for data in results:
                        rv.append("  %s" % self._format_status(test, data).rstrip())
            else:
                for test_id, results in logs.items():
                    test = self._get_file_name(test_id)
                    assert len(results) == 1
                    data = results[0]
                    assert "subtest" not in data
                    rv.append(self._format_status(test, data).rstrip())

        return "\n".join(rv)

    def test_start(self, data):
        action = self.term.yellow(data['action'].upper())
        return "%s: %s" % (action, self._get_test_id(data))

    def test_end(self, data):
        subtests = self._get_subtest_data(data)

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
            if len(unexpected) == 1 and parent_unexpected:
                message = unexpected[0].get("message", "")
                if message:
                    rv += " - %s" % message
                if "stack" in data:
                    rv += self._format_stack(data["stack"])
            elif not self.verbose:
                rv += "\n"
                for d in unexpected:
                    rv += self._format_status(data['test'], d)

        if "expected" not in data and not bool(subtests['unexpected']):
            color = self.term.green
        else:
            color = self.term.red

        action = color(data['action'].upper())
        return "%s: %s" % (action, rv)

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

        if data["status"] == "PASS":
            self.status_buffer[test]["pass"] += 1

        if 'expected' in data:
            self.status_buffer[test]["unexpected"] += 1
        if self.verbose:
            return self._format_status(test, data).rstrip('\n')

    def assertion_count(self, data):
        if data["min_expected"] <= data["count"] <= data["max_expected"]:
            return

        if data["min_expected"] != data["max_expected"]:
            expected = "%i to %i" % (data["min_expected"],
                                     data["max_expected"])
        else:
            expected = "%i" % data["min_expected"]

        action = self.term.red("ASSERT")
        return "%s: Assertion count %i, expected %s assertions\n" % (
                action, data["count"], expected)

    def process_output(self, data):
        rv = []

        pid = data['process']
        if pid.isdigit():
            pid = 'pid:%s' % pid
        pid = self.term.dim_cyan(pid)

        if "command" in data and data["process"] not in self._known_pids:
            self._known_pids.add(data["process"])
            rv.append('%s Full command: %s' % (pid, data["command"]))

        rv.append('%s %s' % (pid, data["data"]))
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

        action = self.term.red(data['action'].upper())
        return "%s: %s" % (action, rv)

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

        if level in ("CRITICAL", "ERROR"):
            level = self.term.red(level)
        elif level == "WARNING":
            level = self.term.yellow(level)
        elif level == "INFO":
            level = self.term.blue(level)

        if data.get('component'):
            rv = " ".join([data["component"], level, data["message"]])
        else:
            rv = "%s %s" % (level, data["message"])

        if "stack" in data:
            rv += "\n%s" % data["stack"]

        return rv

    def lint(self, data):
        fmt = "{path}  {c1}{lineno}{column}  {c2}{level}{normal}  {message}" \
              "  {c1}{rule}({linter}){normal}"
        message = fmt.format(
            path=data["path"],
            normal=self.term.normal,
            c1=self.term.grey,
            c2=self.term.red if data["level"] == 'error' else self.term.yellow,
            lineno=str(data["lineno"]),
            column=(":" + str(data["column"])) if data.get("column") else "",
            level=data["level"],
            message=data["message"],
            rule='{} '.format(data["rule"]) if data.get("rule") else "",
            linter=data["linter"].lower() if data.get("linter") else "",
        )

        return message

    def shutdown(self, data):
        if not self.summary_on_shutdown:
            return

        heading = "Overall Summary"
        rv = ["", self.term.bold_yellow(heading), self.term.bold_yellow("=" * len(heading))]
        for suite, summary in self.summary:
            rv.append(self._format_suite_summary(suite, summary))
        return "\n".join(rv)

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
