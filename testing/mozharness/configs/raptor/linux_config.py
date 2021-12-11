# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os

VENV_PATH = "%s/build/venv" % os.getcwd()

config = {
    "log_name": "raptor",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "title": os.uname()[1].lower().split(".")[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install-chromium-distribution",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
}
