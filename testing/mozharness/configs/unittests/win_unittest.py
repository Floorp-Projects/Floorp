# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os
import platform
import sys

# OS Specifics
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "firefox", "firefox.exe")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.zip")
NODEJS_PATH = None
if "MOZ_FETCHES_DIR" in os.environ:
    NODEJS_PATH = os.path.join(os.environ["MOZ_FETCHES_DIR"], "node/node.exe")

PYWIN32 = "pypiwin32==219"
if sys.version_info > (3, 0):
    PYWIN32 = "pywin32==300"

XPCSHELL_NAME = "xpcshell.exe"
EXE_SUFFIX = ".exe"
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
    "exes": {
        "python": sys.executable,
        "hg": os.path.join(os.environ.get("PROGRAMFILES", ""), "Mercurial", "hg"),
    },
    ###
    "installer_path": INSTALLER_PATH,
    "binary_path": BINARY_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "virtualenv_modules": [PYWIN32, "six==1.13.0", "vcversioner==2.16.0.0"],
    "virtualenv_path": "venv",
    "exe_suffix": EXE_SUFFIX,
    "run_file_names": {
        "mochitest": "runtests.py",
        "reftest": "runreftest.py",
        "xpcshell": "runxpcshelltests.py",
        "cppunittest": "runcppunittests.py",
        "gtest": "rungtests.py",
        "jittest": "jit_test.py",
    },
    "minimum_tests_zip_dirs": [
        "bin/*",
        "certs/*",
        "config/*",
        "mach",
        "marionette/*",
        "modules/*",
        "mozbase/*",
        "tools/*",
        "mozpack/*",
        "mozbuild/*",
    ],
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--utility-path=tests/bin",
                "--xre-path=%(abs_app_dir)s",
            ],
            "run_filename": "runcppunittests.py",
            "testsdir": "cppunittest",
        },
        "jittest": {
            "options": [
                "tests/bin/js",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all",
                "--timeout=970",  # Keep in sync with run_timeout below.
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test",
            "run_timeout": 1000,  # Keep in sync with --timeout above.
        },
        "mochitest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
                "--cleanup-crashes",
                "--marionette-startup-timeout=180",
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest",
        },
        "reftest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--cleanup-crashes",
                "--marionette-startup-timeout=180",
                "--sandbox-read-whitelist=%(abs_work_dir)s",
            ],
            "run_filename": "runreftest.py",
            "testsdir": "reftest",
        },
        "xpcshell": {
            "options": [
                "--self-test",
                "--symbols-path=%(symbols_path)s",
                "--test-plugin-path=%(test_plugin_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--utility-path=tests/bin",
            ],
            "run_filename": "runxpcshelltests.py",
            "testsdir": "xpcshell",
        },
        "gtest": {
            "options": [
                "--xre-path=%(abs_res_dir)s",
                "--cwd=%(gtest_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--utility-path=tests/bin",
                "%(binary_path)s",
            ],
            "run_filename": "rungtests.py",
        },
    },
    # local mochi suites
    "all_mochitest_suites": {
        "mochitest-plain": ["--chunk-by-dir=4"],
        "mochitest-plain-gpu": ["--subsuite=gpu"],
        "mochitest-media": ["--subsuite=media"],
        "mochitest-chrome": ["--flavor=chrome", "--chunk-by-dir=4", "--disable-e10s"],
        "mochitest-chrome-gpu": ["--flavor=chrome", "--subsuite=gpu", "--disable-e10s"],
        "mochitest-browser-chrome": ["--flavor=browser", "--chunk-by-runtime"],
        "mochitest-browser-chrome-screenshots": [
            "--flavor=browser",
            "--subsuite=screenshots",
        ],
        "mochitest-webgl1-core": ["--subsuite=webgl1-core"],
        "mochitest-webgl1-ext": ["--subsuite=webgl1-ext"],
        "mochitest-webgl2-core": ["--subsuite=webgl2-core"],
        "mochitest-webgl2-ext": ["--subsuite=webgl2-ext"],
        "mochitest-webgl2-deqp": ["--subsuite=webgl2-deqp"],
        "mochitest-webgpu": ["--subsuite=webgpu"],
        "mochitest-devtools-chrome": [
            "--flavor=browser",
            "--subsuite=devtools",
            "--chunk-by-runtime",
        ],
        "mochitest-a11y": ["--flavor=a11y", "--disable-e10s"],
        "mochitest-remote": ["--flavor=browser", "--subsuite=remote"],
    },
    # local reftest suites
    "all_reftest_suites": {
        "crashtest": {
            "options": ["--suite=crashtest", "--topsrcdir=tests/reftest/tests"],
            "tests": ["tests/reftest/tests/testing/crashtest/crashtests.list"],
        },
        "jsreftest": {
            "options": [
                "--extra-profile-file=tests/jsreftest/tests/js/src/tests/user.js",
                "--suite=jstestbrowser",
                "--topsrcdir=tests/jsreftest/tests",
            ],
            "tests": ["tests/jsreftest/tests/js/src/tests/jstests.list"],
        },
        "reftest": {
            "options": ["--suite=reftest", "--topsrcdir=tests/reftest/tests"],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"],
        },
        "reftest-no-accel": {
            "options": [
                "--suite=reftest",
                "--setpref=layers.acceleration.disabled=true",
                "--topsrcdir=tests/reftest/tests",
            ],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"],
        },
    },
    "all_xpcshell_suites": {
        "xpcshell": {
            "options": [
                "--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                "--manifest=tests/xpcshell/tests/xpcshell.ini",
            ],
            "tests": [],
        },
    },
    "all_cppunittest_suites": {"cppunittest": ["tests/cppunittest"]},
    "all_gtest_suites": {"gtest": []},
    "all_jittest_suites": {
        "jittest": [],
        "jittest-chunked": [],
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
                "\"&{$p='HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\\Notifications\Settings\Windows.SystemToast.SecurityAndMaintenance';if(!(Test-Path -Path $p)){&New-Item -Path $p -Force}&Set-ItemProperty -Path $p -Name Enabled -Value 0}\"",  # noqa
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
        {
            "name": "prepare chrome profile",
            "cmd": [
                "powershell",
                "-command",
                "if (test-path ${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe) {start chrome; Start-Sleep -s 30; taskkill /F /IM chrome.exe /T}",
            ],
            "architectures": ["32bit", "64bit"],
            "halt_on_failure": True,
            "enabled": True,
        },
    ],
    "vcs_output_timeout": 1000,
    "minidump_save_path": "%(abs_work_dir)s/../minidumps",
    "unstructured_flavors": {
        "xpcshell": [],
        "gtest": [],
        "cppunittest": [],
        "jittest": [],
    },
    "nodejs_path": NODEJS_PATH,
}
