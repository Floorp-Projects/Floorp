# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import cpp
import js
import re
import rust
import sys

from glean_parser import lint, parser, util
from pathlib import Path


def get_parser_options(moz_app_version):
    app_version_major = moz_app_version.split(".", 1)[0]
    return {
        "allow_reserved": False,
        "custom_is_expired": lambda expires: expires == "expired"
        or expires != "never"
        and int(expires) <= int(app_version_major),
        "custom_validate_expires": lambda expires: expires in ("expired", "never")
        or re.fullmatch(r"\d\d+", expires, flags=re.ASCII),
    }


def parse(args):
    """
    Parse and lint the input files,
    then return the parsed objects for further processing.
    """

    # Unfortunately, GeneratedFile appends `flags` directly after `inputs`
    # instead of listifying either, so we need to pull stuff from a *args.
    yaml_array = args[:-1]
    moz_app_version = args[-1]

    input_files = [Path(x) for x in yaml_array]

    # Derived heavily from glean_parser.translate.translate.
    # Adapted to how mozbuild sends us a fd, and to expire on versions not dates.

    options = get_parser_options(moz_app_version)

    # Lint the yaml first, then lint the metrics.
    if lint.lint_yaml_files(input_files, parser_config=options):
        # Warnings are Errors
        sys.exit(1)

    all_objs = parser.parse_objects(input_files, options)
    if util.report_validation_errors(all_objs):
        sys.exit(1)

    nits = lint.lint_metrics(all_objs.value, options)
    if nits is not None and any(nit.check_name != "EXPIRED" for nit in nits):
        # Treat Warnings as Errors in FOG.
        # But don't fail the whole build on expired metrics (it blocks testing).
        sys.exit(1)

    return all_objs.value, options


# Must be kept in sync with the length of `deps` in moz.build.
DEPS_LEN = 13


def main(output_fd, *args):
    args = args[DEPS_LEN:]
    all_objs, options = parse(args)
    rust.output_rust(all_objs, output_fd, options)


def cpp_metrics(output_fd, *args):
    args = args[DEPS_LEN:]
    all_objs, options = parse(args)
    cpp.output_cpp(all_objs, output_fd, options)


def js_metrics(output_fd, *args):
    args = args[DEPS_LEN:]
    all_objs, options = parse(args)
    js.output_js(all_objs, output_fd, options)


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
