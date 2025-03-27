# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import sys
from os import path
from pathlib import Path

import mozunit
from expect_helper import expect

# Shenanigans to import the FOG glean_parser runner
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import jog
import run_glean_parser


def test_jogfile_output():
    """
    A regression test. Very fragile.
    It generates a jogfile for metrics_test.yaml and compares it
    byte-for-byte with an expected output file.

    Also, part one of a two-part test.
    The generated jogfile is consumed in Rust_TestJogfile in t/c/g/tests/gtest/test.rs
    This is to ensure that the jogfile we generate in Python can be consumed in Rust.

    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/tests/pytest
    """

    options = {"allow_reserved": False}
    here_path = Path(path.dirname(__file__))
    input_files = [here_path / "metrics_test.yaml", here_path / "pings_test.yaml"]

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    output_fd = io.StringIO()
    jog.output_file(all_objs, output_fd, options)

    expect(here_path / "jogfile_output", output_fd.getvalue())


if __name__ == "__main__":
    mozunit.main()
