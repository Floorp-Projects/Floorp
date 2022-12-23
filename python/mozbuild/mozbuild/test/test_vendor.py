# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import tempfile
from unittest.mock import Mock

import mozunit
from buildconfig import topsrcdir

from mozbuild.vendor.vendor_python import VendorPython


def test_up_to_date_vendor():
    with tempfile.TemporaryDirectory() as work_dir:
        subprocess.check_call(["hg", "init", work_dir])
        os.makedirs(os.path.join(work_dir, "third_party"))
        shutil.copytree(
            os.path.join(topsrcdir, os.path.join("third_party", "python")),
            os.path.join(work_dir, os.path.join("third_party", "python")),
        )

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
                os.path.join(topsrcdir, os.path.join("third_party", "python")),
                os.path.join(work_dir, os.path.join("third_party", "python")),
                "--exclude=__pycache__",
                "--strip-trailing-cr",
            ]
        )


if __name__ == "__main__":
    mozunit.main()
