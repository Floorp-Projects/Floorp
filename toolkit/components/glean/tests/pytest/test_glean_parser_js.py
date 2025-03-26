# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import sys
from os import path
from pathlib import Path

import mozunit
from expect_helper import expect

# Shenanigans to import the js outputter extension
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser

import js


def test_all_metric_types():
    """Honestly, this is a pretty bad test.
    It generates C++ for a given test metrics.yaml and compares it byte-for-byte
    with an expected output C++ file.
    Expect it to be fragile.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/tests/pytest
    """

    options = {"allow_reserved": False}
    input_files = [
        Path(path.join(path.dirname(__file__), x))
        for x in ["metrics_test.yaml", "metrics2_test.yaml"]
    ]

    interesting = [
        Path(path.join(path.dirname(__file__), x)) for x in ["metrics_test.yaml"]
    ]
    options.update({"interesting": interesting})

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    output_fd_h = io.StringIO()
    output_fd_cpp = io.StringIO()
    js.output_js(all_objs, output_fd_h, output_fd_cpp, options)

    expect(
        path.join(path.dirname(__file__), "metrics_test_output_js_h"),
        output_fd_h.getvalue(),
    )

    expect(
        path.join(path.dirname(__file__), "metrics_test_output_js_cpp"),
        output_fd_cpp.getvalue(),
    )


def test_fake_pings():
    """Another similarly-fragile test.
    It generates C++ for pings_test.yaml, comparing it byte-for-byte
    with an expected output C++ file `pings_test_output_js`.
    Expect it to be fragile.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/tests/pytest
    """

    options = {"allow_reserved": False}
    input_files = [Path(path.join(path.dirname(__file__), "pings_test.yaml"))]

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    output_fd_h = io.StringIO()
    output_fd_cpp = io.StringIO()
    js.output_js(all_objs, output_fd_h, output_fd_cpp, options)

    expect(
        path.join(path.dirname(__file__), "pings_test_output_js_h"),
        output_fd_h.getvalue(),
    )

    expect(
        path.join(path.dirname(__file__), "pings_test_output_js_cpp"),
        output_fd_cpp.getvalue(),
    )


if __name__ == "__main__":
    mozunit.main()
