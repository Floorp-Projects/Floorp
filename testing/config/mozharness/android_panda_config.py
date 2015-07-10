# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=tests/bin",
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--localBinDir=../tests/bin",
                "--apk=%(apk_path)s",
                "--skip-manifest=../tests/cppunittests/android_cppunittest_manifest.txt"
            ],
            "run_filename": "remotecppunittests.py",
            "testsdir": "cppunittest"
        },
        "crashtest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "reftest/tests/testing/crashtest/crashtests.list"
            ],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "jittest": {
            "options": [
                "bin/js",
                "--remote",
                "-j",
                "1",
                "--deviceTransport=sut",
                "--deviceIP=%(device_ip)s",
                "--localLib=../tests/bin",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all"
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test"
        },
        "jsreftest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--extra-profile-file=jsreftest/tests/user.js",
                "jsreftest/tests/jstests.list",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "mochitest": {
            "options": [
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--certificate-path=certs",
                "--app=%(app_name)s",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--screenshot-on-fail",
            ],
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest"
        },
        "reftest": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--app=%(app_name)s",
                "--ignore-window-size",
                "--bootstrap",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "reftest/tests/layout/reftests/reftest.list"
            ],
            "run_filename": "remotereftest.py",
            "testsdir": "reftest"
        },
        "robocop": {
            "options": [
                "--dm_trans=sut",
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--utility-path=../hostutils/bin",
                "--certificate-path=certs",
                "--app=%(app_name)s",
                "--console-level=INFO",
                "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s",
                "--symbols-path=%(symbols_path)s",
                "--robocop-ini=mochitest/robocop.ini"
            ],
            "run_filename": "runrobocop.py",
            "testsdir": "mochitest"
        },
        "xpcshell": {
            "options": [
                "--deviceIP=%(device_ip)s",
                "--xre-path=../hostutils/xre",
                "--manifest=xpcshell/tests/xpcshell.ini",
                "--build-info-json=xpcshell/mozinfo.json",
                "--testing-modules-dir=modules",
                "--local-lib-dir=../fennec",
                "--apk=../%(apk_name)s",
                "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--log-raw=%(raw_log_file)s"
            ],
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell"
        }
    }
}
