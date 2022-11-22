# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from os import path
from pathlib import Path

import mozunit

# Shenanigans to import the metrics index's list of metrics.yamls
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(FOG_ROOT_PATH)
from metrics_index import metrics_yamls, tags_yamls

# Shenanigans to import run_glean_parser
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))
from glean_parser import lint, parser, util


def test_no_metrics_expired():
    """
    Of all the metrics included in this build, are any expired?
    If so, they must be removed or renewed.

    (This also checks other lints, as a treat.)
    """
    with open("browser/config/version.txt", "r") as version_file:
        app_version = version_file.read().strip()

    options = run_glean_parser.get_parser_options(app_version)
    paths = [Path(x) for x in metrics_yamls] + [Path(x) for x in tags_yamls]
    all_objs = parser.parse_objects(paths, options)
    assert not util.report_validation_errors(all_objs)
    assert not lint.lint_metrics(all_objs.value, options)


if __name__ == "__main__":
    mozunit.main()
