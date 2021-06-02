# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import mozunit
from os import path
from pathlib import Path
import re
import sys

from expect_helper import expect

# Shenanigans to import the rust outputter extension
FOG_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir))
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser
import rust

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))
from glean_parser import lint, parser, util


def test_all_metric_types():
    """Honestly, this is a pretty bad test.
    It generates Rust for a given test metrics.yaml and compares it byte-for-byte
    with an expected output Rust file.
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
    rust.output_rust(all_objs.value, output_fd, options)

    expect(
        path.join(path.dirname(__file__), "metrics_test_output"), output_fd.getvalue()
    )


def test_fake_pings():
    """Another similarly-bad test.
    It generates Rust for pings_test.yaml, comparing it byte-for-byte
    with an expected output Rust file.
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
    rust.output_rust(all_objs.value, output_fd, options)

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

    all_objs = parser.parse_objects(input_files, options)

    assert not util.report_validation_errors(all_objs)
    assert not lint.lint_metrics(all_objs.value, options)

    assert all_objs.value["test"]["expired1"].disabled is True
    assert all_objs.value["test"]["expired2"].disabled is True
    assert all_objs.value["test"]["unexpired"].disabled is False


def test_numeric_expires():
    """What if the expires value is a number, not a string?
    This test relies on the intermediary object format output by glean_parser.
    Expect it to be fragile on glean_parser updates that change that format.
    """

    # We'll never get to checking expires values, so this app version shouldn't matter.
    options = run_glean_parser.get_parser_options("42.0a1")
    input_files = [
        Path(path.join(path.dirname(__file__), "metrics_expires_number_test.yaml"))
    ]

    all_objs = parser.parse_objects(input_files, options)
    errors = list(all_objs)
    assert len(errors) == 1
    assert re.search("99 is not of type 'string'", str(errors[0]))


if __name__ == "__main__":
    mozunit.main()
