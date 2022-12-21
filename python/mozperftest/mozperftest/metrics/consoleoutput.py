# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from mozperftest.layers import Layer
from mozperftest.metrics.common import COMMON_ARGS, filtered_metrics

RESULTS_TEMPLATE = """\

==========================================================
                    Results ({})
==========================================================

{}

"""


class ConsoleOutput(Layer):
    """Output metrics in the console."""

    name = "console"
    # By default activate the console layer when running locally.
    activated = "MOZ_AUTOMATION" not in os.environ
    arguments = COMMON_ARGS

    def run(self, metadata):
        # Get filtered metrics
        results = filtered_metrics(
            metadata,
            self.get_arg("output"),
            self.get_arg("prefix"),
            metrics=self.get_arg("metrics"),
            transformer=self.get_arg("transformer"),
            split_by=self.get_arg("split-by"),
            simplify_names=self.get_arg("simplify-names"),
            simplify_exclude=self.get_arg("simplify-exclude"),
        )

        if not results:
            self.warning("No results left after filtering")
            return metadata

        for name, res in results.items():
            # Make a nicer view of the data
            subtests = [
                "{}: {}".format(r["subtest"], [v["value"] for v in r["data"]])
                for r in res
            ]

            # Output the data to console
            self.info(
                "\n==========================================================\n"
                "=                          Results                       =\n"
                "=========================================================="
                "\n" + "\n".join(subtests) + "\n"
            )
        return metadata
