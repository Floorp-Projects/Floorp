# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Default configuration as used by Release Engineering for testing release/beta builds

from __future__ import absolute_import
import os
import sys

import mozharness


external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    "external_tools",
)


config = {
    # General local variable overwrite
    "exes": {
        "gittool.py": [
            # Bug 1227079 - Python executable eeded to get it executed on Windows
            sys.executable,
            os.path.join(external_tools_path, "gittool.py"),
        ],
    },
    # mozcrash support
    "download_symbols": "ondemand",
}
