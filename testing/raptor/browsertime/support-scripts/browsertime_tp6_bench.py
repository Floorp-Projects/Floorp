# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import re

import filters

DOMAIN_MATCHER = re.compile(r"(?:https?:\/\/)?(?:[^@\n]+@)?(?:www\.)?([^:\/\n]+)")
VARIANCE_THRESHOLD = 0.2


def extract_domain(link):
    match = DOMAIN_MATCHER.search(link)
    if match:
        return match.group(1)
    raise Exception(f"Could not find domain for {link}")


class TP6BenchSupport:
    def __init__(self, **kwargs):
        self._load_times = []
        self._total_times = []
        self._geomean_load_times = []
        self._sites_tested = 0

    def setup_test(self, test, args):
        from cmdline import DESKTOP_APPS
        from manifest import get_browser_test_list
        from utils import transform_subtest

        all_tests = get_browser_test_list(args.app, args.run_local)

        manifest_to_find = "browsertime-tp6.ini"
        if args.app not in DESKTOP_APPS:
            manifest_to_find = "browsertime-tp6m.ini"

        test_urls = []
        playback_pageset_manifests = []
        for parsed_test in all_tests:
            if manifest_to_find in parsed_test["manifest"]:
                test_urls.append(parsed_test["test_url"])
                playback_pageset_manifests.append(
                    transform_subtest(
                        parsed_test["playback_pageset_manifest"], parsed_test["name"]
                    )
                )

        test["test_url"] = ",".join(test_urls)
        test["playback_pageset_manifest"] = ",".join(playback_pageset_manifests)

    def modify_command(self, cmd):
        pass

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        """Parse a result for the required results.

        This method handles parsing a new result from Browsertime. The
        expected data returned should follow the following format:
        {
            "custom_data": True,
            "measurements": {
                "fcp": [0, 1, 1, 2, ...],
                "custom-metric-name": [9, 9, 9, 8, ...]
            }
        }

        `bt_result` holds that current results that have been parsed. Add
        new measurements as a dictionary to `bt_result["measurements"]`. Watch
        out for overriding other measurements.

        `raw_result` is a single browser-cycle/iteration from Browsertime. Use object
        attributes to store values across browser-cycles, and produce overall results
        on the last run (denoted by `last_result`).
        """
        measurements = {"totalTime": []}

        # Find new results to add
        for res in raw_result["extras"]:
            if "pageload-benchmark" in res:
                total_time = int(
                    round(res["pageload-benchmark"].get("totalTime", 0), 0)
                )
                measurements["totalTime"].append(total_time)
                self._total_times.append(total_time)

        # Gather the load times of each page/cycle to help with diagnosing issues
        load_times = []
        page_name = None
        page_title = None
        for cycle in raw_result["browserScripts"]:
            if not page_name:
                page_name = extract_domain(cycle["pageinfo"].get("url", ""))

                # Use the page title to differentiate pages with similar domains, and
                # limit it to 35 characters
                page_title = (
                    cycle["pageinfo"].get("documentTitle", "")[:35].replace(" ", "")
                )

            load_time = cycle["timings"]["loadEventEnd"]
            measurements.setdefault(
                f"{page_name} - {page_title} - loadTime", []
            ).append(load_time)

            load_times.append(load_time)

        cur_load_times = self._load_times or [0] * len(load_times)
        self._load_times = list(map(sum, zip(cur_load_times, load_times)))
        self._sites_tested += 1

        for measurement, values in measurements.items():
            bt_result["measurements"].setdefault(measurement, []).extend(values)
        if last_result:
            bt_result["measurements"]["totalLoadTime"] = self._load_times
            bt_result["measurements"]["totalLoadTimePerSite"] = [
                round(total_load_time / self._sites_tested, 2)
                for total_load_time in self._load_times
            ]
            bt_result["measurements"]["totalTimePerSite"] = [
                round(total_time / self._sites_tested, 2)
                for total_time in self._total_times
            ]

    def _build_subtest(self, measurement_name, replicates, test):
        unit = test.get("unit", "ms")
        if test.get("subtest_unit"):
            unit = test.get("subtest_unit")

        return {
            "name": measurement_name,
            "lowerIsBetter": test.get("lower_is_better", True),
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "unit": unit,
            "replicates": replicates,
            "value": filters.geometric_mean(replicates),
        }

    def summarize_test(self, test, suite, **kwargs):
        """Summarize the measurements found in the test as a suite with subtests.

        Note that the same suite will be passed when the test is the same.

        Here's a small example of an expected suite result
        (see performance-artifact-schema.json for more information):
            {
                "name": "pageload-benchmark",
                "type": "pageload",
                "extraOptions": ["fission", "cold", "webrender"],
                "tags": ["fission", "cold", "webrender"],
                "lowerIsBetter": true,
                "unit": "ms",
                "alertThreshold": 2.0,
                "subtests": [{
                    "name": "totalTimePerSite",
                    "lowerIsBetter": true,
                    "alertThreshold": 2.0,
                    "unit": "ms",
                    "shouldAlert": false,
                    "replicates": [
                        6490.47, 6700.73, 6619.47,
                        6823.07, 6541.53, 7152.67,
                        6553.4, 6471.53, 6548.8, 6548.87
                    ],
                    "value": 6553.4
            }

        Some fields are setup by default for the suite:
            {
                "name": test["name"],
                "type": test["type"],
                "extraOptions": extra_options,
                "tags": test.get("tags", []) + extra_options,
                "lowerIsBetter": test["lower_is_better"],
                "unit": test["unit"],
                "alertThreshold": float(test["alert_threshold"]),
                "subtests": {},
            }
        """
        suite["type"] = "pageload"
        if suite["subtests"] == {}:
            suite["subtests"] = []
        for measurement_name, replicates in test["measurements"].items():
            if not replicates:
                continue
            suite["subtests"].append(
                self._build_subtest(measurement_name, replicates, test)
            )
        suite["subtests"].sort(key=lambda subtest: subtest["name"])
