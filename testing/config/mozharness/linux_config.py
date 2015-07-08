# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(abs_app_dir)s"
            ],
            "run_filename": "runcppunittests.py",
            "testsdir": "cppunittest"
        },
        "jittest": {
            "options": [
                "tests/bin/js",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all"
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test"
        },
        "luciddream-emulator": {
            "options": [
                "--startup-timeout=300",
                "--log-raw=%(raw_log_file)s",
                "--browser-path=%(browser_path)s",
                "--b2gpath=%(emulator_path)s",
                "%(test_manifest)s"
            ],
        },
        "luciddream-b2gdt": {
            "options": [
                "--startup-timeout=300",
                "--log-raw=%(raw_log_file)s",
                "--browser-path=%(browser_path)s",
                "--b2g-desktop-path=%(fxos_desktop_path)s",
                "--gaia-profile=%(gaia_profile)s",
                "%(test_manifest)s"
            ],
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
                "--use-test-media-devices",
                "--screenshot-on-fail",
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest"
        },
        "mozbase": {
            "options": [
                "-b",
                "%(binary_path)s"
            ],
            "run_filename": "test.py",
            "testsdir": "mozbase"
        },
        "mozmill": {
            "options": [
                "--binary=%(binary_path)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "runtestlist.py",
            "testsdir": "mozmill"
        },
        "reftest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "runreftest.py",
            "testsdir": "reftest"
        },
        "webapprt": {
            "options": [
                "--app=%(app_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--autorun",
                "--close-when-done",
                "--console-level=INFO",
                "--testing-modules-dir=tests/modules",
                "--quiet"
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest"
        },
        "xpcshell": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--test-plugin-path=%(test_plugin_path)s",
                "--log-raw=%(raw_log_file)s",
                "--utility-path=tests/bin",
            ],
            "run_filename": "runxpcshelltests.py",
            "testsdir": "xpcshell"
        }
    }
}
