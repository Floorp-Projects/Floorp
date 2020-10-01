# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import rust
import sys

from glean_parser import lint, parser, util
from pathlib import Path


def get_parser_options(moz_app_version):
    app_version_major = moz_app_version.split('.', 1)[0]
    return {
        "allow_reserved": False,
        "custom_is_expired":
            lambda expires:
                expires == "expired" or expires != "never" and int(
                    expires) <= int(app_version_major),
        "custom_validate_expires":
            lambda expires:
                expires in ("expired", "never") or re.fullmatch(r"\d\d+", expires, flags=re.ASCII),
    }


def input_files_from_index(metrics_index_path, which_array):
    """
    Get the paths to all input files to look at.

    This select the input files to use by loading the index
    and selecting the appropriate array.
    It then normalizes relatives paths into absolute paths
    based on the `TOPSRCDIR` environment variable.
    """

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

    topsrcdir = Path(os.environ.get("TOPSRCDIR", "."))
    return [topsrcdir / Path(x) for x in input_files]


def main(output_fd, metrics_index_path, which_array, moz_app_version):
    input_files = input_files_from_index(metrics_index_path, which_array)

    # Derived heavily from glean_parser.translate.translate.
    # Adapted to how mozbuild sends us a fd, and to expire on versions not dates.

    options = get_parser_options(moz_app_version)
    all_objs = parser.parse_objects(input_files, options)
    if util.report_validation_errors(all_objs):
        sys.exit(1)

    if lint.lint_metrics(all_objs.value, options):
        # Treat Warnings as Errors in FOG
        sys.exit(1)

    rust.output_rust(all_objs.value, output_fd, options)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
