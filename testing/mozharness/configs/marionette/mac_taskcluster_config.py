# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configuration over-rides for prod_config.py, for osx

# OS Specifics
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = False

#####
config = {
    "tooltool_cache": "/builds/tooltool_cache",
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
                "python",
                "../scripts/external_tools/mouse_and_screen_resolution.py",
                "--configuration-file",
                "../scripts/external_tools/machine-configuration.json",
            ],
            "architectures": ["32bit"],
            "halt_on_failure": True,
            "enabled": ADJUST_MOUSE_AND_SCREEN,
        },
    ],
}
