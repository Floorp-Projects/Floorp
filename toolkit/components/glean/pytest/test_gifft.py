# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import mozunit
from os import path
from pathlib import Path
import sys


# Shenanigans to import the FOG glean_parser runner
FOG_ROOT_PATH = path.abspath(path.join(path.dirname(__file__), path.pardir))
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))
from glean_parser import lint, parser, util


def test_gifft_codegen():
    """
    A regression test. Very fragile.
    It generates C++ for GIFFT for metrics_test.yaml and compares it
    byte-for-byte with expected output C++ files.
    To generate new expected output files, edit t/c/g/metrics_index.py,
    comment out the other metrics yamls, and add one for
    t/c/g/pytest/metrics_test.yaml. Run `mach build` (it'll fail). Copy
    objdir/t/c/g/XGIFFTMap.h to gifft_output_X.
    Don't forget to undo your edits to metrics_index.py after you're done.
    """

    options = {"allow_reserved": False}
    here_path = Path(path.dirname(__file__))
    input_files = [here_path / "metrics_test.yaml"]

    all_objs = parser.parse_objects(input_files, options)
    assert not util.report_validation_errors(all_objs)
    assert not lint.lint_metrics(all_objs.value, options)

    all_objs = all_objs.value
    for probe_type in ("Event", "Histogram", "Scalar"):
        output_fd = io.StringIO()
        cpp_fd = io.StringIO()
        run_glean_parser.output_gifft_map(output_fd, probe_type, all_objs, cpp_fd)

        with open(here_path / f"gifft_output_{probe_type}", "r") as file:
            EXPECTED = file.read()
        assert output_fd.getvalue() == EXPECTED

        if probe_type == "Event":
            with open(here_path / "gifft_output_EventExtra", "r") as file:
                EXPECTED = file.read()
            assert cpp_fd.getvalue() == EXPECTED


if __name__ == "__main__":
    mozunit.main()
