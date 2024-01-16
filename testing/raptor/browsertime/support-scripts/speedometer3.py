# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import filters
from base_python_support import BasePythonSupport
from utils import flatten


class Speedometer3Support(BasePythonSupport):
    def handle_result(self, bt_result, raw_result, **kwargs):
        """Parse a result for the required results.

        See base_python_support.py for what's expected from this method.
        """
        for res in raw_result["extras"]:
            sp3_mean_score = round(res["s3"]["score"]["mean"], 3)
            flattened_metrics_s3_internal = flatten(res["s3_internal"], ())

            clean_flat_internal_metrics = {}
            for k, vals in flattened_metrics_s3_internal.items():
                if k in ("mean", "geomean"):
                    # Skip these for parity with what was being
                    # returned in the results.py/output.py
                    continue
                clean_flat_internal_metrics[k.replace("tests/", "")] = [
                    round(val, 3) for val in vals
                ]

            clean_flat_internal_metrics["score-internal"] = clean_flat_internal_metrics[
                "score"
            ]
            clean_flat_internal_metrics["score"] = [sp3_mean_score]

            for k, v in clean_flat_internal_metrics.items():
                bt_result["measurements"].setdefault(k, []).extend(v)

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

        if "score-internal" in measurement_name:
            subtest["shouldAlert"] = False

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
