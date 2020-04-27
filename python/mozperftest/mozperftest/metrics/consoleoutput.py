# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from mozperftest.metrics.common import filtered_metrics
from mozperftest.layers import Layer


class ConsoleOutput(Layer):
    """Output metrics in the console.
    """

    name = "console"
    activated = False

    arguments = {
        "metrics": {
            "nargs": "*",
            "default": [],
            "help": "The metrics that should be retrieved from the data.",
        },
        # XXX can we guess this by asking the metrics storage ??
        "prefix": {
            "type": str,
            "default": "",
            "help": "Prefix used by the output files.",
        },
    }

    def __call__(self, metadata):
        # Get filtered metrics
        results = filtered_metrics(
            metadata,
            self.get_arg("output"),
            self.get_arg("prefix"),
            self.get_arg("metrics"),
        )

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
