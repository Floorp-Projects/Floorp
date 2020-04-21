# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ..transformer import Transformer


class ReplicateRetriever(Transformer):
    entry_number = 0

    def merge(self, data):
        # Merge data from all subtests
        grouped_data = {}
        for entry in data:
            subtest = entry["subtest"]
            if subtest not in grouped_data:
                grouped_data[subtest] = []
            grouped_data[subtest].append(entry)
        merged_data = []
        for subtest in grouped_data:
            data = [(entry["xaxis"], entry["data"]) for entry in grouped_data[subtest]]

            dsorted = sorted(data, key=lambda t: t[0])

            merged = {"data": [], "xaxis": []}
            for xval, val in dsorted:
                merged["data"].extend(val)
                merged["xaxis"].extend(xval)
            merged["subtest"] = subtest

            merged_data.append(merged)

        self.entry_number = 0
        return merged_data

    def transform(self, data):
        ret = []
        self.entry_number += 1
        for suite_info in data["suites"][0]["subtests"]:
            ret.append(
                {
                    "data": suite_info["replicates"],
                    "xaxis": [self.entry_number] * len(suite_info["replicates"]),
                    "subtest": suite_info["name"],
                }
            )
        return ret
