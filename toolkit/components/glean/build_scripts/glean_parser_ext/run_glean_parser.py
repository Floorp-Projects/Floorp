# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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


def main(output_fd, _metrics_index, *args):

    # Unfortunately, GeneratedFile appends `flags` directly after `inputs`
    # instead of listifying either, so we need to pull stuff from a *args.
    yaml_array = args[:-1]
    moz_app_version = args[-1]

    input_files = [Path(x) for x in yaml_array]

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
