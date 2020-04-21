# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ..transformer import Transformer
from ..utilities import flat


class SingleJsonRetriever(Transformer):
    """Transforms perfherder data into the standardized data format.
    """

    entry_number = 0

    def transform(self, data):
        self.entry_number += 1

        # flat(data, ()) returns a dict that have one key per dictionary path
        # in the original data.
        return [
            {
                "data": [{"value": i, "xaxis": self.entry_number} for i in v],
                "subtest": k,
            }
            for k, v in flat(data, ()).items()
        ]

    def merge(self, sde):
        grouped_data = {}
        for entry in sde:
            subtest = entry["subtest"]
            data = grouped_data.get(subtest, [])
            data.extend(entry["data"])
            grouped_data.update({subtest: data})

        merged_data = [{"data": v, "subtest": k} for k, v in grouped_data.items()]

        self.entry_number = 0
        return merged_data
