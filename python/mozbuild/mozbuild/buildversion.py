# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from pathlib import Path

from packaging.version import Version


def mozilla_build_version():
    mozilla_build = os.environ.get("MOZILLABUILD")

    version_file = Path(mozilla_build) / "VERSION"

    assert version_file.exists(), (
        f'The MozillaBuild VERSION file was not found at "{version_file}".\n'
        "Please check if MozillaBuild is installed correctly and that the"
        "`MOZILLABUILD` environment variable is to the correct path."
    )

    with version_file.open() as file:
        return Version(file.readline().rstrip("\n"))
