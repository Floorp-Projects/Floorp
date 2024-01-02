# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a template config file for marionette production.
HG_SHARE_BASE_DIR = "/builds/hg-shared"

# OS Specifics
DISABLE_SCREEN_SAVER = True
ADJUST_MOUSE_AND_SCREEN = False

#####
config = {
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.toml",
    "vcs_share_base": HG_SHARE_BASE_DIR,
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/worker/tooltool-cache",
    "suite_definitions": {
        "marionette_desktop": {
            "options": [
                "-vv",
                "--log-errorsummary=%(error_summary_file)s",
                "--log-html=%(html_report_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s",
            ],
            "run_filename": "",
            "testsdir": "marionette",
        }
    },
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        # NOTE 'enabled' is only here while we have unconsolidated configs
        {
            "name": "disable_screen_saver",
            "cmd": ["xset", "s", "off", "s", "reset"],
            "halt_on_failure": False,
            "architectures": ["32bit", "64bit"],
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
    "structured_output": True,
}
