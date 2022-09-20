# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
from os import path
import sys

# Shenanigans to import the metrics index's lists of yamls
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(FOG_ROOT_PATH)
from metrics_index import metrics_yamls, pings_yamls, tags_yamls


def test_yamls_sorted(yamls_to_test=[metrics_yamls, pings_yamls, tags_yamls]):
    """
    Ensure the yamls indices are sorted lexicographically.
    """
    for yamls in yamls_to_test:
        assert sorted(yamls) == yamls, "FOG YAMLs must be lexicographically sorted"


if __name__ == "__main__":
    mozunit.main()
