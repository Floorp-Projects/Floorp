# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib

from mozperftest.layers import Layer
from mozperftest.metrics.common import COMMON_ARGS, filtered_metrics
from mozperftest.metrics.notebook import PerftestNotebook
from mozperftest.metrics.utils import is_number


class Notebook(Layer):
    """Post standarized data to iodide and run analysis."""

    name = "notebook"
    activated = False

    arguments = COMMON_ARGS
    arguments.update(
        {
            "analysis": {
                "nargs": "*",
                "default": [],
                "help": "List of analyses to run in Iodide.",
            },
            "analyze-strings": {
                "action": "store_true",
                "default": False,
                "help": (
                    "If set, strings won't be filtered out of the results to analyze in Iodide."
                ),
            },
            "no-server": {
                "action": "store_true",
                "default": False,
                "help": "If set, the data won't be opened in Iodide.",
            },
            "compare-to": {
                "nargs": "*",
                "default": [],
                "help": (
                    "Compare the results from this test to the historical data in the folder(s) "
                    "specified through this option. Only JSON data can be processed for the "
                    "moment. Each folder containing those JSONs is considered as a distinct "
                    "data point to compare with the newest run."
                ),
            },
            "stats": {
                "action": "store_true",
                "default": False,
                "help": "If set, browsertime statistics will be reported.",
            },
        }
    )

    def run(self, metadata):
        exclusions = None
        if not self.get_arg("stats"):
            exclusions = ["statistics."]

        for result in metadata.get_results():
            result["name"] += "- newest run"

        analysis = self.get_arg("analysis")
        dir_list = self.get_arg("compare-to")
        if dir_list:
            analysis.append("compare")
            for directory in dir_list:
                dirpath = pathlib.Path(directory)
                if not dirpath.exists():
                    raise Exception(f"{dirpath} does not exist.")
                if not dirpath.is_dir():
                    raise Exception(f"{dirpath} is not a directory")
                # TODO: Handle more than just JSON data.
                for jsonfile in dirpath.rglob("*.json"):
                    metadata.add_result(
                        {
                            "results": str(jsonfile.resolve()),
                            "name": jsonfile.parent.name,
                        }
                    )

        results = filtered_metrics(
            metadata,
            self.get_arg("output"),
            self.get_arg("prefix"),
            metrics=self.get_arg("metrics"),
            transformer=self.get_arg("transformer"),
            exclude=exclusions,
            split_by=self.get_arg("split-by"),
            simplify_names=self.get_arg("simplify-names"),
            simplify_exclude=self.get_arg("simplify-exclude"),
        )

        if not results:
            self.warning("No results left after filtering")
            return metadata

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
        self.ptnb.post_to_iodide(
            analysis, start_local_server=not self.get_arg("no-server")
        )

        return metadata
