# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a template config file for marionette production on Windows.
import os
import platform
import sys

# OS Specifics
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = True
DESKTOP_VISUALFX_THEME = {
    "Let Windows choose": 0,
    "Best appearance": 1,
    "Best performance": 2,
    "Custom": 3,
}.get("Best appearance")
TASKBAR_AUTOHIDE_REG_PATH = {
    "Windows 7": "HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StuckRects2",
    "Windows 10": "HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StuckRects3",
}.get("{} {}".format(platform.system(), platform.release()))

#####
config = {
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.toml",
    "virtualenv_path": "venv",
    "exes": {
        "python": sys.executable,
        "hg": os.path.join(os.environ["PROGRAMFILES"], "Mercurial", "hg"),
    },
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "install",
        "run-tests",
    ],
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
        },
    },
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
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
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": ADJUST_MOUSE_AND_SCREEN,
        },
        {
            "name": "disable windows security and maintenance notifications",
            "cmd": [
                "powershell",
                "-command",
                r"\"&{$p='HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Notifications\Settings\Windows.SystemToast.SecurityAndMaintenance';if(!(Test-Path -Path $p)){&New-Item -Path $p -Force}&Set-ItemProperty -Path $p -Name Enabled -Value 0}\"",  # noqa
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": (platform.release() == 10),
        },
        {
            "name": "set windows VisualFX",
            "cmd": [
                "powershell",
                "-command",
                "\"&{{&Set-ItemProperty -Path 'HKCU:Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' -Name VisualFXSetting -Value {}}}\"".format(
                    DESKTOP_VISUALFX_THEME
                ),
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True,
        },
        {
            "name": "create scrollbars always show key",
            "cmd": [
                "powershell",
                "-command",
                "New-ItemProperty -Path 'HKCU:\Control Panel\Accessibility' -Name 'DynamicScrollbars' -Value 0",
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": False,
            "enabled": True,
        },
        {
            "name": "hide windows taskbar",
            "cmd": [
                "powershell",
                "-command",
                "\"&{{$p='{}';$v=(Get-ItemProperty -Path $p).Settings;$v[8]=3;&Set-ItemProperty -Path $p -Name Settings -Value $v}}\"".format(
                    TASKBAR_AUTOHIDE_REG_PATH
                ),
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True,
        },
        {
            "name": "restart windows explorer",
            "cmd": [
                "powershell",
                "-command",
                '"&{&Stop-Process -ProcessName explorer}"',
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True,
        },
    ],
}
