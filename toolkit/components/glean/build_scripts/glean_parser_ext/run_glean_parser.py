# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import rust
import sys

from glean_parser import lint, parser, util
from pathlib import Path


def main(output_fd, metrics_index_path, which_array):

    # Source the list of input files from `metrics_index.py`
    sys.path.append(str(Path(metrics_index_path).parent))
    from metrics_index import METRICS, PINGS
    if which_array == 'METRICS':
        input_files = METRICS
    elif which_array == 'PINGS':
        input_files = PINGS
    else:
        print("Build system's asking for unknown array {}".format(which_array))
        sys.exit(1)

    # Derived heavily from glean_parser.translate.translate.
    # Adapted to how mozbuild sends us a fd.
    options = {"allow_reserved": False}
    input_files = [Path(x) for x in input_files]

    all_objs = parser.parse_objects(input_files, options)
    if util.report_validation_errors(all_objs):
        sys.exit(1)

    if lint.lint_metrics(all_objs.value, options):
        # Treat Warnings as Errors in FOG
        sys.exit(1)

    rust.output_rust(all_objs.value, output_fd, options)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
