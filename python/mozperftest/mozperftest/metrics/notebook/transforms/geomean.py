# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from scipy.stats import gmean

from ..transformer import Transformer


class GeomeanTransformer(Transformer):
    """Transforms perfherder data into the standardized data format.
    """

    entry_number = 0

    def transform(self, data):
        self.entry_number += 1

        fcpval = 0
        loadtval = 0
        for entry in data["suites"][0]["subtests"]:
            if "fcp" in entry["name"]:
                fcpval = entry["value"]
            elif "loadtime" in entry["name"]:
                loadtval = entry["value"]

        return {"data": [gmean([fcpval, loadtval])], "xaxis": self.entry_number}

    def merge(self, sde):
        merged = {"data": [], "xaxis": []}

        for entry in sde:
            if type(entry["xaxis"]) in (dict, list):
                raise Exception(
                    "Expecting non-iterable data type in xaxis entry, found %s"
                    % type(entry["xaxis"])
                )

        data = [(entry["xaxis"], entry["data"]) for entry in sde]

        dsorted = sorted(data, key=lambda t: t[0])

        for xval, val in dsorted:
            merged["data"].extend(val)
            merged["xaxis"].append(xval)

        self.entry_number = 0
        return merged
