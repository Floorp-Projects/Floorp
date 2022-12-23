#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import pathlib
import shutil
import subprocess
import tempfile
import zipfile
import buildconfig


def main():
    if not buildconfig.substs.get("MOZ_CODE_COVERAGE") or not buildconfig.substs.get(
        "MOZ_RUST_TESTS"
    ):
        return

    assert (
        "GRCOV_PATH" in os.environ
    ), "The environment variable GRCOV_PATH should contain a path to grcov"
    grcov_path = os.environ["GRCOV_PATH"]
    assert os.path.exists(grcov_path), "grcov should exist"

    grcov_command = [
        grcov_path,
        "-t",
        "lcov",
        "-p",
        buildconfig.topsrcdir,
        buildconfig.topobjdir,
    ]

    if buildconfig.substs["OS_TARGET"] == "Linux":
        gcc_dir = os.path.join(os.environ["MOZ_FETCHES_DIR"], "gcc")
        if "LD_LIBRARY_PATH" in os.environ:
            os.environ["LD_LIBRARY_PATH"] = "{}/lib64/:{}".format(
                gcc_dir, os.environ["LD_LIBRARY_PATH"]
            )
        else:
            os.environ["LD_LIBRARY_PATH"] = "{}/lib64/".format(gcc_dir)

        os.environ["PATH"] = "{}/bin/{}{}".format(
            gcc_dir, os.pathsep, os.environ["PATH"]
        )

    grcov_output = subprocess.check_output(grcov_command)

    grcov_zip_path = os.path.join(buildconfig.topobjdir, "code-coverage-grcov.zip")
    with zipfile.ZipFile(grcov_zip_path, "a", zipfile.ZIP_DEFLATED) as z:
        z.writestr("grcov_lcov_output.info", grcov_output)


if __name__ == "__main__":
    main()
