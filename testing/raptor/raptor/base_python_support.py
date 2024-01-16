# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class BasePythonSupport:
    def __init__(self, **kwargs):
        pass

    def setup_test(self, test, args):
        """Used to setup the test.

        The `test` arg is the test itself with all of its current settings.
        It can be modified as needed to add additional information to the
        test that will run.

        The `args` arg contain all the user-specified args, or the default
        settings for them. These can be useful for changing the behaviour
        based on the app, or if we're running locally.

        No return is expected. The `test` argument can be changed directly.
        """
        pass

    def modify_command(self, cmd):
        """Used to modify the Browsertime command before running the test.

        The `cmd` arg holds the current browsertime command to run. It can
        be changed directly to change how browsertime runs.
        """
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
        pass

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
        pass

    def summarize_suites(self, suites):
        """Used to summarize all the suites.

        The `suites` arg provides all the suites that were built
        in this test run. This method can be used to modify those,
        or to create new ones based on the others. For instance,
        it can be used to create "duplicate" suites that use
        different methods for the summary value.

        Note that the subtest/suite names should be changed if
        existing suites are duplicated so that no conflicts arise
        during perfherder data ingestion.
        """
        pass
