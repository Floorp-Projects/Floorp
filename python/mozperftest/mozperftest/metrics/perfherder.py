# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import statistics

from mozperftest.layers import Layer
from mozperftest.metrics.common import CommonMetricsSingleton
from mozperftest.metrics.utils import write_json, filter_metrics


class Perfherder(Layer):
    """Output data in the perfherder format.
    """

    name = "perfherder"
    activated = False

    arguments = {
        "prefix": {
            "type": str,
            "default": "",
            "help": "Prefix the output files with this string.",
        },
        "metrics": {
            "nargs": "*",
            "default": [],
            "help": "The metrics that should be retrieved from the data.",
        },
    }

    def __call__(self, metadata):
        """Processes the given results into a perfherder-formatted data blob.

        If the `--perfherder` flag isn't provided, then the
        results won't be processed into a perfherder-data blob. If the
        flavor is unknown to us, then we assume that it comes from
        browsertime.

        XXX If needed, make a way to do flavor-specific processing

        :param results list/dict/str: Results to process.
        :param perfherder bool: True if results should be processed
            into a perfherder-data blob.
        :param flavor str: The flavor that is being processed.
        """
        # Get the common requirements for metrics (i.e. output path,
        # results to process)
        cm = CommonMetricsSingleton(
            metadata.get_result(),
            self.warning,
            output=self.get_arg("output"),
            prefix=self.get_arg("perfherder-prefix"),
        )
        res = cm.get_standardized_data(
            group_name="firefox", transformer="SingleJsonRetriever"
        )
        _, results = res["file-output"], res["data"]

        # Filter out unwanted metrics
        results = filter_metrics(results, self.get_arg("perfherder-metrics"))
        if not results:
            self.warning("No results left after filtering")
            return metadata

        # XXX Instead of just passing replicates here, we should build
        # up a partial perfherder data blob (with options) and subtest
        # overall values.
        subtests = {
            res["subtest"]: [v["value"] for v in res["data"]] for res in results
        }

        # XXX Pass options into this function and use those instead
        # of the defaults provided below.
        perfherder_data = self._build_blob(subtests)

        file = "perfherder-data.json"
        if cm.prefix:
            file = "{}-{}".format(cm.prefix, file)
        self.info(
            "Writing perfherder results to {}".format(os.path.join(cm.output, file))
        )

        # XXX "suites" key error occurs when using self.info so a print
        # is being done for now.
        print("PERFHERDER_DATA: " + json.dumps(perfherder_data))
        metadata.set_output(write_json(perfherder_data, cm.output, file))
        return metadata

    def _build_blob(
        self,
        subtests,
        test_type="pageload",
        extra_options=None,
        should_alert=False,
        subtest_should_alert=None,
        suiteshould_alert=False,
        framework=None,
        application=None,
        alert_threshold=2.0,
        lower_is_better=True,
        unit="ms",
        summary=None,
    ):
        """Build a PerfHerder data blob from the given subtests.

        NOTE: This is a WIP, see the many TODOs across this file.

        Given a dictionary of subtests, and the values. Build up a
        perfherder data blob. Note that the naming convention for
        these arguments is different then the rest of the scripts
        to make it easier to see where they are going to in the perfherder
        data.

        For the `should_alert` field, if should_alert is True but `subtest_should_alert`
        is empty, then all subtests along with the suite will generate alerts.
        Otherwise, if the subtest_should_alert contains subtests to alert on, then
        only those will alert and nothing else (including the suite). If the
        suite value should alert, then set `suiteshould_alert` to True.

        :param subtests dict: A dictionary of subtests and the values.
            XXX TODO items for subtests:
                (1) Allow it to contain replicates and individual settings
                    for each of the subtests.
                (2) The geomean of the replicates will be taken for now,
                    but it should be made more flexible in some way.
                (3) We need some way to handle making multiple suites.
        :param test_type str: The type of test that was run.
        :param extra_options list: A list of extra options to store.
        :param should_alert bool: Whether all values in the suite should
            generate alerts or not.
        :param subtest_should_alert list: A list of subtests to alert on. If this
            is not empty, then it will disable the suite-level alerts.
        :param suiteshould_alert bool: Used if `subtest_should_alert` is not
            empty, and if True, then the suite-level value will generate
            alerts.
        :param framework dict: Information about the framework that
            is being tested.
        :param application dict: Information about the application that
            is being tested. Must include name, and optionally a version.
        :param alert_threshold float: The change in percentage this
            metric must undergo to to generate an alert.
        :param lower_is_better bool: If True, then lower values are better
            than higher ones.
        :param unit str: The unit of the data.
        :param summary float: The summary value to use in the perfherder
            data blob. By default, the mean of all the subtests will be
            used.

        :return dict: The PerfHerder data blob.
        """
        if extra_options is None:
            extra_options = []
        if subtest_should_alert is None:
            subtest_should_alert = []
        if framework is None:
            framework = {"name": "mozperftest"}
        if application is None:
            application = {"name": "Firefox", "version": "9000"}

        perf_subtests = []
        suite = {
            "name": "btime-testing",
            "type": test_type,
            "value": None,
            "unit": unit,
            "extraOptions": extra_options,
            "lowerIsBetter": lower_is_better,
            "alertThreshold": alert_threshold,
            "shouldAlert": (should_alert and not subtest_should_alert)
            or suiteshould_alert,
            "subtests": perf_subtests,
        }

        perfherder = {
            "suites": [suite],
            "framework": framework,
            "application": application,
        }

        allvals = []
        for measurement in subtests:
            reps = subtests[measurement]
            allvals.extend(reps)

            if len(reps) == 0:
                self.warning("No replicates found for {}, skipping".format(measurement))
                continue

            perf_subtests.append(
                {
                    "name": measurement,
                    "replicates": reps,
                    "lowerIsBetter": lower_is_better,
                    "value": statistics.mean(reps),
                    "unit": unit,
                    "shouldAlert": should_alert or measurement in subtest_should_alert,
                }
            )

        if len(allvals) == 0:
            raise Exception(
                "Could not build perfherder data blob because no data was provided"
            )

        suite["value"] = statistics.mean(allvals)
        return perfherder
