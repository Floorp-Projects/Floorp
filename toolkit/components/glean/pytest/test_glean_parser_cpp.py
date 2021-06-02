# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import mozunit
from os import path
from pathlib import Path
import sys

from expect_helper import expect

# Shenanigans to import the cpp outputter extension
FOG_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir))
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import cpp

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))
from glean_parser import lint, parser, util


def test_all_metric_types():
    """Honestly, this is a pretty bad test.
    It generates C++ for a given test metrics.yaml and compares it byte-for-byte
    with an expected output C++ file.
    Expect it to be fragile.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/pytest
    """

    options = {"allow_reserved": False}
    input_files = [Path(path.join(path.dirname(__file__), "metrics_test.yaml"))]

    all_objs = parser.parse_objects(input_files, options)
    assert not util.report_validation_errors(all_objs)
    assert not lint.lint_metrics(all_objs.value, options)

    output_fd = io.StringIO()
    cpp.output_cpp(all_objs.value, output_fd, options)

    expect(
        path.join(path.dirname(__file__), "metrics_test_output_cpp"),
        output_fd.getvalue(),
    )


def test_fake_pings():
    """Another similarly-fragile test.
    It generates C++ for pings_test.yaml, comparing it byte-for-byte
    with an expected output C++ file `pings_test_output_cpp`.
    Expect it to be fragile.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/pytest
    """

    options = {"allow_reserved": False}
    input_files = [Path(path.join(path.dirname(__file__), "pings_test.yaml"))]

    all_objs = parser.parse_objects(input_files, options)
    assert not util.report_validation_errors(all_objs)
    assert not lint.lint_metrics(all_objs.value, options)

    output_fd = io.StringIO()
    cpp.output_cpp(all_objs.value, output_fd, options)

    expect(
        path.join(path.dirname(__file__), "pings_test_output_cpp"), output_fd.getvalue()
    )


if __name__ == "__main__":
    mozunit.main()
