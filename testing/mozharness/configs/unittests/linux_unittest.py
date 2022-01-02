# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os
import platform

# OS Specifics
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "application", "firefox", "firefox-bin")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.tar.bz2")
XPCSHELL_NAME = "xpcshell"
HTTP3SERVER_NAME = "http3server"
EXE_SUFFIX = ""
DISABLE_SCREEN_SAVER = True
ADJUST_MOUSE_AND_SCREEN = False

# Note: keep these Valgrind .sup file names consistent with those
# in testing/mochitest/mochitest_options.py.
VALGRIND_SUPP_DIR = os.path.join(os.getcwd(), "build/tests/mochitest")
NODEJS_PATH = None
if "MOZ_FETCHES_DIR" in os.environ:
    NODEJS_PATH = os.path.join(os.environ["MOZ_FETCHES_DIR"], "node/bin/node")

VALGRIND_SUPP_CROSS_ARCH = os.path.join(VALGRIND_SUPP_DIR, "cross-architecture.sup")
VALGRIND_SUPP_ARCH = None

if platform.architecture()[0] == "64bit":
    VALGRIND_SUPP_ARCH = os.path.join(VALGRIND_SUPP_DIR, "x86_64-pc-linux-gnu.sup")
else:
    VALGRIND_SUPP_ARCH = os.path.join(VALGRIND_SUPP_DIR, "i386-pc-linux-gnu.sup")

#####
config = {
    ###
    "virtualenv_modules": ["six==1.13.0", "vcversioner==2.16.0.0"],
    "installer_path": INSTALLER_PATH,
    "binary_path": BINARY_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "http3server_name": HTTP3SERVER_NAME,
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
                "--setpref=webgl.force-enabled=true",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--use-test-media-devices",
                "--screenshot-on-fail",
                "--cleanup-crashes",
                "--marionette-startup-timeout=180",
                "--sandbox-read-whitelist=%(abs_work_dir)s",
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
        "mochitest-valgrind-plain": [
            "--valgrind=/usr/bin/valgrind",
            "--valgrind-supp-files="
            + VALGRIND_SUPP_ARCH
            + ","
            + VALGRIND_SUPP_CROSS_ARCH,
            "--timeout=900",
            "--max-timeouts=50",
        ],
        "mochitest-plain": ["--chunk-by-dir=4"],
        "mochitest-plain-gpu": ["--subsuite=gpu"],
        "mochitest-plain-coverage": ["--chunk-by-dir=4", "--timeout=1200"],
        "mochitest-media": ["--subsuite=media"],
        "mochitest-chrome": ["--flavor=chrome", "--chunk-by-dir=4", "--disable-e10s"],
        "mochitest-chrome-gpu": ["--flavor=chrome", "--subsuite=gpu", "--disable-e10s"],
        "mochitest-browser-chrome": ["--flavor=browser", "--chunk-by-runtime"],
        "mochitest-browser-chrome-coverage": [
            "--flavor=browser",
            "--chunk-by-runtime",
            "--timeout=1200",
        ],
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
        "mochitest-devtools-chrome-coverage": [
            "--flavor=browser",
            "--subsuite=devtools",
            "--chunk-by-runtime",
            "--timeout=1200",
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
            "options": [
                "--suite=reftest",
                "--setpref=layers.acceleration.force-enabled=true",
                "--topsrcdir=tests/reftest/tests",
            ],
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
        "reftest-snapshot": {
            "options": [
                "--suite=reftest",
                "--setpref=reftest.use-draw-snapshot=true",
                "--topsrcdir=tests/reftest/tests",
            ],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"],
        },
    },
    "all_xpcshell_suites": {
        "xpcshell": {
            "options": [
                "--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                "--http3server=%(abs_app_dir)s/" + HTTP3SERVER_NAME,
                "--manifest=tests/xpcshell/tests/xpcshell.ini",
            ],
            "tests": [],
        },
        "xpcshell-coverage": {
            "options": [
                "--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                "--http3server=%(abs_app_dir)s/" + HTTP3SERVER_NAME,
                "--manifest=tests/xpcshell/tests/xpcshell.ini",
                "--sequential",
            ],
            "tests": [],
        },
    },
    "all_cppunittest_suites": {"cppunittest": {"tests": ["tests/cppunittest"]}},
    "all_gtest_suites": {"gtest": []},
    "all_jittest_suites": {
        "jittest": [],
        "jittest1": ["--total-chunks=2", "--this-chunk=1"],
        "jittest2": ["--total-chunks=2", "--this-chunk=2"],
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
    "vcs_output_timeout": 1000,
    "minidump_save_path": "%(abs_work_dir)s/../minidumps",
    "unstructured_flavors": {
        "xpcshell": [],
        "gtest": [],
        "cppunittest": [],
        "jittest": [],
    },
    "tooltool_cache": "/builds/worker/tooltool-cache",
    "nodejs_path": NODEJS_PATH,
    # "log_format": "%(levelname)8s - %(message)s",
}
