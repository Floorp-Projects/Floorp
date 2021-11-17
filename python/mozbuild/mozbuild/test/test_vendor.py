# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shutil
import subprocess
import tempfile

from buildconfig import topsrcdir
from unittest.mock import Mock
from mozbuild.vendor.vendor_python import VendorPython
import mozunit


def test_up_to_date_vendor():
    with tempfile.TemporaryDirectory() as work_dir:
        subprocess.check_call(["hg", "init", work_dir])
        os.makedirs(os.path.join(work_dir, "build"))
        os.makedirs(os.path.join(work_dir, "third_party"))

        # Create empty virtualenv_packages file
        with open(
            os.path.join(work_dir, "build", "common_virtualenv_packages.txt"), "a"
        ) as file:
            # Since VendorPython thinks "work_dir" is the topsrcdir,
            # it will use its associated virtualenv and package configuration.
            # Add `pip-tools` and its dependencies.
            file.write("vendored:third_party/python/Click\n")
            file.write("vendored:third_party/python/pip\n")
            file.write("vendored:third_party/python/pip_tools\n")
            file.write("vendored:third_party/python/setuptools\n")
            file.write("vendored:third_party/python/wheel\n")

        # Copy existing "third_party/python/" vendored files
        existing_vendored = os.path.join(topsrcdir, "third_party", "python")
        work_vendored = os.path.join(work_dir, "third_party", "python")
        shutil.copytree(existing_vendored, work_vendored)

        # Copy "mach" module so that `CommandSiteManager` can populate itself.
        # This is needed because "topsrcdir" is used in this test both for determining
        # import paths and for acting as a "work dir".
        existing_mach = os.path.join(topsrcdir, "python", "mach")
        work_mach = os.path.join(work_dir, "python", "mach")
        shutil.copytree(existing_mach, work_mach)

        # Run the vendoring process
        vendor = VendorPython(
            work_dir, None, Mock(), topobjdir=os.path.join(work_dir, "obj")
        )
        vendor.vendor()

        # Verify that re-vendoring did not cause file changes.
        # Note that we don't want hg-ignored generated files
        # to bust the diff, so we exclude them (pycache, egg-info).
        subprocess.check_call(
            [
                "diff",
                "-r",
                existing_vendored,
                work_vendored,
                "--exclude=__pycache__",
            ]
        )


if __name__ == "__main__":
    mozunit.main()
