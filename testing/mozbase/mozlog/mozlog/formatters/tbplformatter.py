# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from .base import BaseFormatter
from .process import strstatus


class TbplFormatter(BaseFormatter):
    """Formatter that formats logs in the legacy formatting format used by TBPL
    This is intended to be used to preserve backward compatibility with existing tools
    hand-parsing this format.
    """
    def __init__(self):
        self.suite_start_time = None
        self.test_start_times = {}

    def __call__(self, data):
        return getattr(self, data["action"])(data)

    def log(self, data):
        if data.get('component'):
            message = "%s %s" % (data["component"], data["message"])
        else:
            message = data["message"]

        if "stack" in data:
            message += "\n%s" % data["stack"]

        return "%s\n" % message

    def process_output(self, data):
        return "PROCESS | %(process)s | %(data)s\n" % data

    def process_start(self, data):
        msg = "TEST-INFO | started process %s" % data['process']
        if 'command' in data:
            msg = '%s (%s)' % (msg, data['command'])
        return msg + '\n'

    def process_exit(self, data):
        return "TEST-INFO | %s: %s\n" % (data['process'],
                                         strstatus(data['exitcode']))

    def crash(self, data):
        id = self.id_str(data["test"]) if "test" in data else "pid: %s" % data["process"]

        signature = data["signature"] if data["signature"] else "unknown top frame"
        rv = ["PROCESS-CRASH | %s | application crashed [%s]" % (id, signature)]

        if data.get("minidump_path"):
            rv.append("Crash dump filename: %s" % data["minidump_path"])

        if data.get("stackwalk_stderr"):
            rv.append("stderr from minidump_stackwalk:")
            rv.append(data["stackwalk_stderr"])
        elif data.get("stackwalk_stdout"):
            rv.append(data["stackwalk_stdout"])

        if data.get("stackwalk_returncode", 0) != 0:
            rv.append("minidump_stackwalk exited with return code %d" %
                      data["stackwalk_returncode"])

        if data.get("stackwalk_errors"):
            rv.extend(data.get("stackwalk_errors"))

        rv = "\n".join(rv)
        if not rv[-1] == "\n":
            rv += "\n"

        return rv

    def suite_start(self, data):
        self.suite_start_time = data["time"]
        return "SUITE-START | Running %i tests\n" % len(data["tests"])

    def test_start(self, data):
        self.test_start_times[self.test_id(data["test"])] = data["time"]

        return "TEST-START | %s\n" % self.id_str(data["test"])

    def test_status(self, data):
        message = "- " + data["message"] if "message" in data else ""
        if "stack" in data:
            message += "\n%s" % data["stack"]
        if message and message[-1] == "\n":
            message = message[:-1]

        if "expected" in data:
            if not message:
                message = "- expected %s" % data["expected"]
            failure_line = "TEST-UNEXPECTED-%s | %s | %s %s\n" % (
                data["status"], self.id_str(data["test"]), data["subtest"],
                message)
            if data["expected"] != "PASS":
                info_line = "TEST-INFO | expected %s\n" % data["expected"]
                return failure_line + info_line
            return failure_line

        return "TEST-%s | %s | %s %s\n" % (
            data["status"], self.id_str(data["test"]), data["subtest"],
            message)

    def test_end(self, data):
        test_id = self.test_id(data["test"])
        duration_msg = ""

        if test_id in self.test_start_times:
            start_time = self.test_start_times.pop(test_id)
            time = data["time"] - start_time
            duration_msg = "took %ims" % time

        if "expected" in data:
            message = data.get("message", "")
            if not message:
                message = "expected %s" % data["expected"]
            if "stack" in data:
                message += "\n%s" % data["stack"]
            if message and message[-1] == "\n":
                message = message[:-1]

            failure_line = "TEST-UNEXPECTED-%s | %s | %s\n" % (
                data["status"], self.id_str(test_id), message)

            if data["expected"] not in ("PASS", "OK"):
                expected_msg = "expected %s | " % data["expected"]
            else:
                expected_msg = ""
            info_line = "TEST-INFO %s%s\n" % (expected_msg, duration_msg)

            return failure_line + info_line

        sections = ["TEST-%s" % data['status'], self.id_str(test_id)]
        if duration_msg:
            sections.append(duration_msg)
        return ' | '.join(sections) + '\n'

    def suite_end(self, data):
        start_time = self.suite_start_time
        time = int((data["time"] - start_time) / 1000)

        return "SUITE-END | took %is\n" % time

    def test_id(self, test_id):
        if isinstance(test_id, (str, unicode)):
            return test_id
        else:
            return tuple(test_id)

    def id_str(self, test_id):
        if isinstance(test_id, (str, unicode)):
            return test_id
        else:
            return " ".join(test_id)

    def valgrind_error(self, data):
        rv = "TEST-VALGRIND-ERROR | " + data['primary'] + "\n"
        for line in data['secondary']:
            rv = rv + line + "\n"

        return rv
