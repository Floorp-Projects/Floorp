# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# XXX Bug 1181261 - Please update config in testing/mozharness/config
# instead. This file is still needed for mulet mochitests, but should
# be removed once bug 1188330 is finished.

config = {
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--dm_trans=adb",
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(xre_path)s",
                "--addEnv",
                "LD_LIBRARY_PATH=/vendor/lib:/system/lib:/system/b2g",
                "--with-b2g-emulator=%(b2gpath)s",
                "."
            ],
            "run_filename": "remotecppunittests.py",
            "testsdir": "cppunittest"
        },
        "crashtest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "tests/testing/crashtest/crashtests.list"
            ],
            "run_filename": "runreftestb2g.py",
            "testsdir": "reftest"
        },
        "jsreftest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--extra-profile-file=jsreftest/tests/user.js",
                "jsreftest/tests/jstests.list"
            ],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "mochitest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--certificate-path=%(certificate_path)s",
                "--screenshot-on-fail",
                "%(test_path)s"
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "mochitest-chrome": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--quiet",
                "--chrome",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--certificate-path=%(certificate_path)s",
                "--screenshot-on-fail",
                "%(test_path)s"
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--emulator-res=800x1000",
                "--logdir=%(logcat_dir)s",
                "--remote-webserver=%(remote_webserver)s",
                "--ignore-window-size",
                "--xre-path=%(xre_path)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--enable-oop",
                "tests/layout/reftests/reftest.list"
            ],
            "run_filename": "runreftestsb2g.py",
            "testsdir": "reftest"
        },
        "xpcshell": {
            "options": [
                "--adbpath=%(adbpath)s",
                "--b2gpath=%(b2gpath)s",
                "--emulator=%(emulator)s",
                "--logdir=%(logcat_dir)s",
                "--manifest=tests/xpcshell.ini",
                "--use-device-libs",
                "--testing-modules-dir=%(modules_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--busybox=%(busybox)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
            ],
            "run_filename": "runtestsb2g.py",
            "testsdir": "xpcshell"
        }
    }
}
