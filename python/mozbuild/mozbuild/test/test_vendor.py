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
            os.path.join(work_dir, "build", "build_virtualenv_packages.txt"), "a"
        ) as file:
            # Since VendorPython thinks "work_dir" is the topsrcdir,
            # it will use its associated virtualenv and package configuration.
            # Since it uses "pip-tools" within, and "pip-tools" needs
            # the "Click" library, we need to make it available.
            file.write("pth:third_party/python/Click")

        # Copy existing "third_party/python/" vendored files
        existing_vendored = os.path.join(topsrcdir, "third_party", "python")
        work_vendored = os.path.join(work_dir, "third_party", "python")
        shutil.copytree(existing_vendored, work_vendored)

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
                "--exclude=*.egg-info",
            ]
        )


if __name__ == "__main__":
    mozunit.main()
