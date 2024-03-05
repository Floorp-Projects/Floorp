# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import filters
from base_python_support import BasePythonSupport


class MotionMarkSupport(BasePythonSupport):
    def handle_result(self, bt_result, raw_result, **kwargs):
        """Parse a result for the required results.

        See base_python_support.py for what's expected from this method.
        """
        suite_name = raw_result["extras"][0]["mm_res"]["suite_name"]
        score_tracker = {
            subtest: []
            for subtest in raw_result["extras"][0]["mm_res"]["results"][
                suite_name
            ].keys()
        }

        motionmark_overall_score = []
        for res in raw_result["extras"]:
            motionmark_overall_score.append(round(res["mm_res"]["score"], 3))

            for k, v in res["mm_res"]["results"][suite_name].items():
                score_tracker[k].append(v["complexity"]["bootstrap"]["median"])

        for k, v in score_tracker.items():
            bt_result["measurements"][k] = v

        bt_result["measurements"]["score"] = motionmark_overall_score

    def _build_subtest(self, measurement_name, replicates, test):
        unit = test.get("unit", "ms")
        if test.get("subtest_unit"):
            unit = test.get("subtest_unit")

        lower_is_better = test.get(
            "subtest_lower_is_better", test.get("lower_is_better", True)
        )
        if "score" in measurement_name:
            lower_is_better = False
            unit = "score"

        subtest = {
            "unit": unit,
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "lowerIsBetter": lower_is_better,
            "name": measurement_name,
            "replicates": replicates,
            "value": round(filters.mean(replicates), 3),
        }

        return subtest

    def summarize_test(self, test, suite, **kwargs):
        """Summarize the measurements found in the test as a suite with subtests.

        See base_python_support.py for what's expected from this method.
        """
        suite["type"] = "benchmark"
        if suite["subtests"] == {}:
            suite["subtests"] = []
        for measurement_name, replicates in test["measurements"].items():
            if not replicates:
                continue
            suite["subtests"].append(
                self._build_subtest(measurement_name, replicates, test)
            )
        suite["subtests"].sort(key=lambda subtest: subtest["name"])

        score = 0
        for subtest in suite["subtests"]:
            if subtest["name"] == "score":
                score = subtest["value"]
                break
        suite["value"] = score

    def modify_command(self, cmd, test):
        """Modify the browsertime command to have the appropriate suite name.

        This is necessary to grab the correct CSS selector in the browsertime
        script, and later for parsing through the final benchmark data in the
        support python script (this file).

        Current options are `MotionMark` and `HTML suite`.
        """

        cmd += ["--browsertime.suite_name", test.get("suite_name")]
