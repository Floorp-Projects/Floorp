# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from .base import BaseFormatter

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
        if "expected" in data:
            failure_line = "TEST-UNEXPECTED-%s | %s | %s %s" % (
                data["status"], self.id_str(data["test"]), data["subtest"],
                message)
            info_line = "TEST-INFO | expected %s\n" % data["expected"]
            return "\n".join([failure_line, info_line])

        return "TEST-%s | %s | %s %s\n" % (
            data["status"], self.id_str(data["test"]), data["subtest"],
            message)

    def test_end(self, data):
        test_id = self.test_id(data["test"])
        time_msg = ""

        if test_id in self.test_start_times:
            start_time = self.test_start_times.pop(test_id)
            time = data["time"] - start_time
            time_msg = " | took %ims" % time

        if "expected" in data:
            failure_line = "TEST-UNEXPECTED-%s | %s | %s" % (
                data["status"], test_id, data.get("message", ""))

            info_line = "TEST-INFO expected %s%s\n" % (data["expected"], time_msg)
            return "\n".join([failure_line, info_line])

        return "TEST-%s | %s%s\n" % (
            data["status"], test_id, time_msg)

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
