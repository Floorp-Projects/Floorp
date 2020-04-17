# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.layers import Layer
from mozperftest.metrics.common import CommonMetricsSingleton
from mozperftest.metrics.utils import filter_metrics


class ConsoleOutput(Layer):
    def __call__(self, metadata):
        """Processes the given results into a perfherder-formatted data blob.

        If the `--perfherder` flag isn't provided, then the
        results won't be processed into a perfherder-data blob. If the
        flavor is unknown to us, then we assume that it comes from
        browsertime.

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
            prefix=self.get_arg("prefix"),
        )
        res = cm.get_standardized_data(
            group_name="firefox", transformer="SingleJsonRetriever"
        )
        _, results = res["file-output"], res["data"]

        # Filter out unwanted metrics
        results = filter_metrics(results, self.get_arg("metrics"))
        if not results:
            self.warning("No results left after filtering")
            return metadata

        # Make a nicer view of the data
        subtests = [
            "{}: {}".format(res["subtest"], [r["value"] for r in res["data"]])
            for res in results
        ]

        # Output the data to console
        self.info(
            "\n==========================================================\n"
            "=                          Results                       =\n"
            "=========================================================="
            "\n" + "\n".join(subtests) + "\n"
        )
        return metadata
