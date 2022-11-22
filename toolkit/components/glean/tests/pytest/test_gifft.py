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
import run_glean_parser


def test_gifft_codegen():
    """
    A regression test. Very fragile.
    It generates C++ for GIFFT for metrics_test.yaml and compares it
    byte-for-byte with expected output C++ files.
    To generate new expected output files, set `UPDATE_EXPECT=1` when running the test suite:

    UPDATE_EXPECT=1 mach test toolkit/components/glean/pytest
    """

    options = {"allow_reserved": False}
    here_path = Path(path.dirname(__file__))
    input_files = [here_path / "metrics_test.yaml"]

    all_objs, options = run_glean_parser.parse_with_options(input_files, options)

    for probe_type in ("Event", "Histogram", "Scalar"):
        output_fd = io.StringIO()
        cpp_fd = io.StringIO()
        run_glean_parser.output_gifft_map(output_fd, probe_type, all_objs, cpp_fd)

        expect(here_path / f"gifft_output_{probe_type}", output_fd.getvalue())

        if probe_type == "Event":
            expect(here_path / "gifft_output_EventExtra", cpp_fd.getvalue())


if __name__ == "__main__":
    mozunit.main()
