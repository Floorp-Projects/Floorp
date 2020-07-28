# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys


def main(output_fd, *metrics_yamls):
    from pathlib import Path

    srcdir = Path(__file__).joinpath('../../../../../../')
    glean_parser_path = srcdir.joinpath('third_party/python/glean_parser')
    sys.path.insert(0, str(glean_parser_path.resolve()))

    from glean_parser import lint, parser, util
    import rust

    # Derived heavily from glean_parser.translate.translate.
    # Adapted to how mozbuild sends us a fd.
    options = {"allow_reserved": False}
    input_files = [Path(x) for x in metrics_yamls]

    all_objs = parser.parse_objects(input_files, options)
    if util.report_validation_errors(all_objs):
        sys.exit(1)

    if lint.lint_metrics(all_objs.value, options):
        # Treat Warnings as Errors in FOG
        sys.exit(1)

    rust.output_rust(all_objs.value, output_fd, options)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
