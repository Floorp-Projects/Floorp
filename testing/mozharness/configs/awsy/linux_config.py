# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

PYTHON = "/usr/bin/env python"
VENV_PATH = "%s/build/venv" % os.getcwd()
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "application", "firefox", "firefox-bin")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.tar.bz2")

config = {
    "log_name": "awsy",
    "binary_path": BINARY_PATH,
    "installer_path": INSTALLER_PATH,
    "virtualenv_path": VENV_PATH,
    "cmd_timeout": 6500,
    "exes": {},
    "title": os.uname()[1].lower().split(".")[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": os.path.join(os.getcwd(), "tooltool_cache"),
}
