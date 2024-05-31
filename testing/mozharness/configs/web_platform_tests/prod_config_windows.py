# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

# This is a template config file for web-platform-tests test.

import os
import sys

# OS Specifics
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = True
#####

REQUIRE_GPU = False
if "REQUIRE_GPU" in os.environ:
    REQUIRE_GPU = os.environ["REQUIRE_GPU"] == "1"

config = {
    "options": [
        "--prefs-root=%(test_path)s\\prefs",
        "--config=%(test_path)s\\wptrunner.ini",
        "--ca-cert-path=%(test_path)s\\tests\\tools\\certs\\cacert.pem",
        "--host-key-path=%(test_path)s\\tests\\tools\\certs\\web-platform.test.key",
        "--host-cert-path=%(test_path)s\\tests\\tools\\certs\\web-platform.test.pem",
        "--certutil-binary=%(test_install_path)s\\bin\\certutil.exe",
    ],
    "exes": {
        "python": sys.executable,
        "hg": "c:\\mozilla-build\\hg\\hg",
    },
    "geckodriver": os.path.join("%(abs_fetches_dir)s", "geckodriver.exe"),
    "per_test_category": "web-platform",
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        # NOTE 'enabled' is only here while we have unconsolidated configs
        {
            "name": "disable_screen_saver",
            "cmd": ["xset", "s", "off", "s", "reset"],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": False,
            "enabled": DISABLE_SCREEN_SAVER,
        },
        {
            "name": "run mouse & screen adjustment script",
            "cmd": [
                # when configs are consolidated this python path will only show
                # for windows.
                sys.executable,
                "..\\scripts\\external_tools\\mouse_and_screen_resolution.py",
                "--configuration-file",
                "..\\scripts\\external_tools\\machine-configuration.json",
            ],
            "architectures": ["32bit"],
            "halt_on_failure": True,
            "enabled": ADJUST_MOUSE_AND_SCREEN,
        },
        {
            "name": "ensure proper graphics driver",
            "cmd": [
                "powershell",
                "-command",
                'if (-Not ((Get-CimInstance win32_VideoController).InstalledDisplayDrivers | Out-String).contains("nvgrid")) { echo "Missing nvgrid driver: " + (Get-CimInstance win32_VideoController).InstalledDisplayDrivers; exit 4; }',
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True if REQUIRE_GPU else False,
            "fatal_exit_code": 4,
        },
    ],
}
