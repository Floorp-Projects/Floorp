# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy
import re

import filters
from base_python_support import BasePythonSupport
from utils import bool_from_str

DOMAIN_MATCHER = re.compile(r"(?:https?:\/\/)?(?:[^@\n]+@)?(?:www\.)?([^:\/\n]+)")
VARIANCE_THRESHOLD = 0.2


def extract_domain(link):
    match = DOMAIN_MATCHER.search(link)
    if match:
        return match.group(1)
    raise Exception(f"Could not find domain for {link}")


class TP6BenchSupport(BasePythonSupport):
    def __init__(self, **kwargs):
        self._load_times = []
        self._total_times = []
        self._geomean_load_times = []
        self._sites_tested = 0
        self._test_pages = {}

    def setup_test(self, test, args):
        from cmdline import DESKTOP_APPS
        from manifest import get_browser_test_list
        from utils import transform_subtest

        all_tests = get_browser_test_list(args.app, args.run_local)

        manifest_to_find = "browsertime-tp6.toml"
        if args.app not in DESKTOP_APPS:
            manifest_to_find = "browsertime-tp6m.toml"

        test_urls = []
        playback_pageset_manifests = []
        for parsed_test in all_tests:
            if manifest_to_find in parsed_test["manifest"]:
                if not bool_from_str(parsed_test.get("benchmark_page", "false")):
                    continue
                test_url = parsed_test["test_url"]
                test_urls.append(test_url)
                playback_pageset_manifests.append(
                    transform_subtest(
                        parsed_test["playback_pageset_manifest"], parsed_test["name"]
                    )
                )
                self._test_pages[test_url] = parsed_test

        if len(playback_pageset_manifests) == 0:
            raise Exception("Could not find any manifests for testing.")

        test["test_url"] = ",".join(test_urls)
        test["playback_pageset_manifest"] = ",".join(playback_pageset_manifests)

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
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
        result_name = None
        for cycle in raw_result["browserScripts"]:
            if not result_name:
                # When the test name is unknown, we use the url TLD combined
                # with the page title to differentiate pages with similar domains, and
                # limit it to 35 characters
                page_url = cycle["pageinfo"].get("url", "")
                if self._test_pages.get(page_url, None) is not None:
                    result_name = self._test_pages[page_url]["name"]
                else:
                    page_name = extract_domain(page_url)
                    page_title = (
                        cycle["pageinfo"].get("documentTitle", "")[:35].replace(" ", "")
                    )
                    result_name = f"{page_name} - {page_title}"

            load_time = cycle["timings"]["loadEventEnd"]
            fcp = cycle["timings"]["paintTiming"]["first-contentful-paint"]
            lcp = (
                cycle["timings"]
                .get("largestContentfulPaint", {})
                .get("renderTime", None)
            )
            measurements.setdefault(f"{result_name} - loadTime", []).append(load_time)

            measurements.setdefault(f"{result_name} - fcp", []).append(fcp)

            if lcp is not None:
                measurements.setdefault(f"{result_name} - lcp", []).append(lcp)

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
            "value": round(filters.geometric_mean(replicates), 3),
        }

    def summarize_test(self, test, suite, **kwargs):
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

    def _produce_suite_alts(self, suite_base, subtests, suite_name_prefix):
        geomean_suite = copy.deepcopy(suite_base)
        geomean_suite["subtests"] = copy.deepcopy(subtests)
        median_suite = copy.deepcopy(geomean_suite)
        median_suite["subtests"] = copy.deepcopy(subtests)

        subtest_values = []
        for subtest in subtests:
            subtest_values.extend(subtest["replicates"])

        geomean_suite["name"] = suite_name_prefix + "-geomean"
        geomean_suite["value"] = filters.geometric_mean(subtest_values)
        for subtest in geomean_suite["subtests"]:
            subtest["value"] = filters.geometric_mean(subtest["replicates"])

        median_suite["name"] = suite_name_prefix + "-median"
        median_suite["value"] = filters.median(subtest_values)
        for subtest in median_suite["subtests"]:
            subtest["value"] = filters.median(subtest["replicates"])

        return [
            geomean_suite,
            median_suite,
        ]

    def summarize_suites(self, suites):
        fcp_subtests = []
        lcp_subtests = []
        load_time_subtests = []

        for suite in suites:
            for subtest in suite["subtests"]:
                if "- fcp" in subtest["name"]:
                    fcp_subtests.append(subtest)
                elif "- lcp" in subtest["name"]:
                    lcp_subtests.append(subtest)
                elif "- loadTime" in subtest["name"]:
                    load_time_subtests.append(subtest)

        fcp_bench_suites = self._produce_suite_alts(
            suites[0], fcp_subtests, "fcp-bench"
        )

        lcp_bench_suites = self._produce_suite_alts(
            suites[0], lcp_subtests, "lcp-bench"
        )

        load_time_bench_suites = self._produce_suite_alts(
            suites[0], load_time_subtests, "loadtime-bench"
        )

        overall_suite = copy.deepcopy(suites[0])
        new_subtests = [
            subtest
            for subtest in suites[0]["subtests"]
            if subtest["name"].startswith("total")
        ]
        overall_suite["subtests"] = new_subtests

        new_suites = [
            overall_suite,
            *fcp_bench_suites,
            *lcp_bench_suites,
            *load_time_bench_suites,
        ]
        suites.pop()
        suites.extend(new_suites)
