# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from machformatter import BaseMachFormatter

class TbplFormatter(BaseMachFormatter):
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
            return "%s %s\n" % (data["component"], data["message"])

        return "%s\n" % (data["message"])

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
        start_time = self.test_start_times.pop(self.test_id(data["test"]))
        time = data["time"] - start_time

        if "expected" in data:
            failure_line = "TEST-UNEXPECTED-%s | %s | %s" % (
                data["status"], self.id_str(data["test"]),
                data.get("message", ""))
            info_line = "TEST-INFO expected %s | took %ims\n" % (data["expected"], time)
            return "\n".join([failure_line, info_line])

        return "TEST-%s | %s | took %ims\n" % (
            data["status"], self.id_str(data["test"]), time)

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
