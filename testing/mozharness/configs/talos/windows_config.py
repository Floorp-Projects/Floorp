# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform
import socket
import sys

PYTHON = sys.executable
VENV_PATH = os.path.join(os.getcwd(), "build/venv")

config = {
    "log_name": "talos",
    "installer_path": "installer.exe",
    "virtualenv_path": VENV_PATH,
    "exes": {
        "python": PYTHON,
        "hg": os.path.join(os.environ["PROGRAMFILES"], "Mercurial", "hg"),
        "tooltool.py": [
            PYTHON,
            os.path.join(os.environ["MOZILLABUILD"], "tooltool.py"),
        ],
    },
    "title": socket.gethostname().split(".")[0],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "populate-webroot",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": os.path.join("c:\\", "build", "tooltool_cache"),
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        {
            "name": "run mouse & screen adjustment script",
            "cmd": [
                sys.executable,
                os.path.join(
                    os.getcwd(),
                    "mozharness",
                    "external_tools",
                    "mouse_and_screen_resolution.py",
                ),
                "--configuration-file",
                os.path.join(
                    os.getcwd(),
                    "mozharness",
                    "external_tools",
                    "machine-configuration.json",
                ),
                "--platform=win10-hw"
                if (platform.uname().version == "10.0.19045")
                else "--platform=win11-hw"
                if (platform.uname().version == "10.0.22621")
                else "--platform=win7",
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True,
        }
    ],
}
