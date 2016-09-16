# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

from base import BaseFormatter

class ErrorSummaryFormatter(BaseFormatter):
    def __init__(self):
        self.line_count = 0

    def __call__(self, data):
        rv = BaseFormatter.__call__(self, data)
        self.line_count += 1
        return rv

    def _output(self, data_type, data):
        data["action"] = data_type
        data["line"] = self.line_count
        return "%s\n" % json.dumps(data)

    def _output_test(self, test, subtest, item):
        data = {"test": test,
                "subtest": subtest,
                "status": item["status"],
                "expected": item["expected"],
                "message": item.get("message"),
                "stack": item.get("stack")}
        return self._output("test_result", data)

    def test_status(self, item):
        if "expected" not in item:
            return
        return self._output_test(item["test"], item["subtest"], item)

    def test_end(self, item):
        if "expected" not in item:
            return
        return self._output_test(item["test"], None, item)

    def log(self, item):
        if item["level"] not in ("ERROR", "CRITICAL"):
            return

        data = {"level": item["level"],
                "message": item["message"]}
        return self._output("log", data)

    def crash(self, item):
        data = {"test": item.get("test"),
                "signature": item["signature"],
                "stackwalk_stdout": item.get("stackwalk_stdout"),
                "stackwalk_stderr": item.get("stackwalk_stderr")}
        return self._output("crash", data)

    def lint(self, item):
        data = {
            "level": item["level"],
            "path": item["path"],
            "message": item["message"],
            "lineno": item["lineno"],
            "column": item.get("column"),
            "rule": item.get("rule"),
            "linter": item.get("linter")
        }
        self._output("lint", data)
