# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os
import socket
import sys

PYTHON = sys.executable
PYTHON_DLL = "c:/mozilla-build/python27/python27.dll"
VENV_PATH = os.path.join(os.getcwd(), "build/venv")

PYWIN32 = "pypiwin32==219"
if sys.version_info > (3, 0):
    PYWIN32 = "pywin32==300"

config = {
    "log_name": "raptor",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "virtualenv_modules": [PYWIN32, "raptor", "mozinstall"],
    "exes": {
        "python": PYTHON,
        "easy_install": [
            "%s/scripts/python" % VENV_PATH,
            "%s/scripts/easy_install-2.7-script.py" % VENV_PATH,
        ],
        "mozinstall": [
            "%s/scripts/python" % VENV_PATH,
            "%s/scripts/mozinstall-script.py" % VENV_PATH,
        ],
        "hg": os.path.join(os.environ["PROGRAMFILES"], "Mercurial", "hg"),
    },
    "title": socket.gethostname().split(".")[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install-chromium-distribution",
        "install",
        "run-tests",
    ],
    "tooltool_cache": os.path.join("c:\\", "build", "tooltool_cache"),
    "python3_manifest": {
        "win32": "python3.manifest",
        "win64": "python3_x64.manifest",
    },
    "env": {
        # python3 requires C runtime, found in firefox installation; see bug 1361732
        "PATH": "%(PATH)s;c:\\slave\\test\\build\\application\\firefox;"
    },
}
