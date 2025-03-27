# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import sys
from os import path
from pathlib import Path

import mozunit
from expect_helper import expect

# Shenanigans to import the rust outputter extension
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser
import rust

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))


def test_all_metric_types():
    """Honestly, this is a pretty bad test.
    It generates Rust for a given test metrics.yaml and compares it byte-for-byte
    with an expected output Rust file.
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

    output_fd = io.StringIO()
    rust.output_rust(all_objs, output_fd, {}, options)

    expect(
        path.join(path.dirname(__file__), "metrics_test_output"), output_fd.getvalue()
    )


def test_fake_pings():
    """Another similarly-bad test.
    It generates Rust for pings_test.yaml, comparing it byte-for-byte
    with an expected output Rust file.
    Expect it to be fragile.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/tests/pytest
    """

    options = {"allow_reserved": False}
    input_files = [Path(path.join(path.dirname(__file__), "pings_test.yaml"))]

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    output_fd = io.StringIO()
    rust.output_rust(all_objs, output_fd, {}, options)

    expect(path.join(path.dirname(__file__), "pings_test_output"), output_fd.getvalue())


def test_expires_version():
    """This test relies on the intermediary object format output by glean_parser.
    Expect it to be fragile on glean_parser updates that change that format.
    """

    # The test file has 41, 42, 100. Use 42.0a1 here to ensure "expires == version" means expired.
    options = run_glean_parser.get_parser_options("42.0a1")
    input_files = [
        Path(path.join(path.dirname(__file__), "metrics_expires_versions_test.yaml"))
    ]

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    assert all_objs["test"]["expired1"].disabled is True
    assert all_objs["test"]["expired2"].disabled is True
    assert all_objs["test"]["unexpired"].disabled is False


if __name__ == "__main__":
    mozunit.main()
