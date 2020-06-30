# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from mozperftest.layers import Layer
from mozperftest.metrics.common import filtered_metrics
from mozperftest.metrics.notebook import PerftestNotebook
from mozperftest.metrics.utils import is_number


class Notebook(Layer):
    """Post standarized data to iodide and run analysis."""

    name = "notebook"
    activated = False

    arguments = {
        "metrics": {
            "nargs": "*",
            "default": [],
            "help": "The metrics that should be retrieved from the data.",
        },
        "prefix": {
            "type": str,
            "default": "",
            "help": "Prefix used by the output files.",
        },
        "analysis": {
            "nargs": "*",
            "default": [],
            "help": "List of analyses to run in Iodide.",
        },
        "analyze-strings": {
            "action": "store_true",
            "default": False,
            "help": (
                "If set, strings won't be filtered out of the results to"
                " analyze in Iodide."
            ),
        },
    }

    def run(self, metadata):
        # Get filtered metrics
        results = filtered_metrics(
            metadata,
            self.get_arg("output"),
            self.get_arg("prefix"),
            metrics=self.get_arg("metrics"),
        )

        if not results:
            self.warning("No results left after filtering")
            return metadata

        analysis = self.get_arg("analysis")
        data_to_post = []
        for name, res in results.items():
            for r in res:
                val = r["data"][0]["value"]
                if is_number(val):
                    data_to_post.append(r)
                elif self.get_arg("analyze-strings"):
                    data_to_post.append(r)
        self.ptnb = PerftestNotebook(
            data=data_to_post, logger=metadata, prefix=self.get_arg("prefix")
        )
        self.ptnb.post_to_iodide(analysis)

        return metadata
