# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from os import path

import mozunit

# Shenanigans to import the metrics index's lists of yamls
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(FOG_ROOT_PATH)
import metrics_index


def test_yamls_sorted():
    """
    Ensure the yamls indices are sorted lexicographically.
    """
    # Ignore lists that are the concatenation of others.
    to_ignore = ["metrics_yamls", "pings_yamls"]

    # Fetch names of all variables defined in the `metrics_index` module.
    yaml_lists = [
        item
        for item in dir(metrics_index)
        if isinstance(getattr(metrics_index, item), list) and not item.startswith("__")
    ]
    for name in yaml_lists:
        if name in to_ignore:
            continue

        yamls_to_test = metrics_index.__dict__[name]
        assert (
            sorted(yamls_to_test) == yamls_to_test
        ), f"{name} must be be lexicographically sorted."


if __name__ == "__main__":
    mozunit.main()
