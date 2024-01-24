# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
from collections import defaultdict

from .base import BaseFormatter


class ErrorSummaryFormatter(BaseFormatter):
    def __init__(self):
        self.test_to_group = {}
        self.groups = defaultdict(
            lambda: {
                "status": None,
                "test_times": [],
                "start": None,
                "end": None,
            }
        )
        self.line_count = 0
        self.dump_passing_tests = False

        if os.environ.get("MOZLOG_DUMP_ALL_TESTS", False):
            self.dump_passing_tests = True

    def __call__(self, data):
        rv = BaseFormatter.__call__(self, data)
        self.line_count += 1
        return rv

    def _output(self, data_type, data):
        data["action"] = data_type
        data["line"] = self.line_count
        return "%s\n" % json.dumps(data)

    def _output_test(self, test, subtest, item):
        data = {
            "test": test,
            "subtest": subtest,
            "group": self.test_to_group.get(test, ""),
            "status": item["status"],
            "expected": item["expected"],
            "message": item.get("message"),
            "stack": item.get("stack"),
            "known_intermittent": item.get("known_intermittent", []),
        }
        return self._output("test_result", data)

    def _get_group_result(self, group, item):
        group_info = self.groups[group]
        result = group_info["status"]

        if result == "ERROR":
            return result

        # If status == expected, we delete item[expected]
        test_status = item["status"]
        test_expected = item.get("expected", test_status)
        known_intermittent = item.get("known_intermittent", [])

        if test_status == "SKIP":
            self.groups[group]["start"] = None
            if result is None:
                result = "SKIP"
        elif test_status == test_expected or test_status in known_intermittent:
            # If the test status is expected, or it's a known intermittent
            # the group has at least one passing test
            result = "OK"
        else:
            result = "ERROR"

        return result

    def _clean_test_name(self, test):
        retVal = test
        # remove extra stuff like "(finished)"
        if "(finished)" in test:
            retVal = test.split(" ")[0]
        return retVal

    def suite_start(self, item):
        self.test_to_group = {v: k for k in item["tests"] for v in item["tests"][k]}
        return self._output("test_groups", {"groups": list(item["tests"].keys())})

    def suite_end(self, data):
        output = []
        for group, info in self.groups.items():
            duration = sum(info["test_times"])

            output.append(
                self._output(
                    "group_result",
                    {
                        "group": group,
                        "status": info["status"],
                        "duration": duration,
                    },
                )
            )

        return "".join(output)

    def test_start(self, item):
        group = item.get(
            "group", self.test_to_group.get(self._clean_test_name(item["test"]), None)
        )
        if group and self.groups[group]["start"] is None:
            self.groups[group]["start"] = item["time"]

    def test_status(self, item):
        group = item.get(
            "group", self.test_to_group.get(self._clean_test_name(item["test"]), None)
        )
        if group:
            self.groups[group]["status"] = self._get_group_result(group, item)

        if not self.dump_passing_tests and "expected" not in item:
            return

        if item.get("expected", "") == "":
            item["expected"] = item["status"]

        return self._output_test(
            self._clean_test_name(item["test"]), item["subtest"], item
        )

    def test_end(self, item):
        group = item.get(
            "group", self.test_to_group.get(self._clean_test_name(item["test"]), None)
        )
        if group:
            self.groups[group]["status"] = self._get_group_result(group, item)
            if self.groups[group]["start"]:
                self.groups[group]["test_times"].append(
                    item["time"] - self.groups[group]["start"]
                )
                self.groups[group]["start"] = None

        if not self.dump_passing_tests and "expected" not in item:
            return

        if item.get("expected", "") == "":
            item["expected"] = item["status"]

        return self._output_test(self._clean_test_name(item["test"]), None, item)

    def log(self, item):
        if item["level"] not in ("ERROR", "CRITICAL"):
            return

        data = {"level": item["level"], "message": item["message"]}
        return self._output("log", data)

    def shutdown_failure(self, item):
        data = {"status": "FAIL", "test": item["group"], "message": item["message"]}
        data["group"] = [g for g in self.groups if item["group"].endswith(g)][0]
        self.groups[data["group"]]["status"] = "FAIL"
        return self._output("log", data)

    def crash(self, item):
        data = {
            "test": item.get("test"),
            "signature": item["signature"],
            "stackwalk_stdout": item.get("stackwalk_stdout"),
            "stackwalk_stderr": item.get("stackwalk_stderr"),
        }

        if item.get("test"):
            data["group"] = self.test_to_group.get(
                self._clean_test_name(item["test"]), ""
            )
            if data["group"] == "":
                # item['test'] could be the group name, not a test name
                if self._clean_test_name(item["test"]) in self.groups:
                    data["group"] = self._clean_test_name(item["test"])

            # unlike test group summary, if we crash expect error unless expected
            if (
                (
                    "expected" in item
                    and "status" in item
                    and item["status"] in item["expected"]
                )
                or ("expected" in item and "CRASH" == item["expected"])
                or "status" in item
                and item["status"] in item.get("known_intermittent", [])
            ):
                self.groups[data["group"]]["status"] = "PASS"
            else:
                self.groups[data["group"]]["status"] = "ERROR"

        return self._output("crash", data)

    def lint(self, item):
        data = {
            "level": item["level"],
            "path": item["path"],
            "message": item["message"],
            "lineno": item["lineno"],
            "column": item.get("column"),
            "rule": item.get("rule"),
            "linter": item.get("linter"),
        }
        self._output("lint", data)
