# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "mochitest_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--utility-path=../hostutils/bin", "--certificate-path=certs",
        "--app=%(app_name)s", "--console-level=INFO",
        "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
        "--run-only-tests=android.json", "--symbols-path=%(symbols_path)s",
        "--quiet"
    ],
    "reftest_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--utility-path=../hostutils/bin",
        "--app=%(app_name)s",
        "--ignore-window-size", "--bootstrap",
        "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
        "--symbols-path=%(symbols_path)s",
        "reftest/tests/layout/reftests/reftest.list"
    ],
    "crashtest_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--utility-path=../hostutils/bin",
        "--app=%(app_name)s",
        "--enable-privilege", "--ignore-window-size", "--bootstrap",
        "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
        "--symbols-path=%(symbols_path)s",
        "reftest/tests/testing/crashtest/crashtests.list"
    ],
    "jsreftest_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--utility-path=../hostutils/bin",
        "--app=%(app_name)s",
        "--enable-privilege", "--ignore-window-size", "--bootstrap",
        "--extra-profile-file=jsreftest/tests/user.js", "jsreftest/tests/jstests.list",
        "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
        "--symbols-path=%(symbols_path)s"
    ],
    "robocop_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--utility-path=../hostutils/bin",
        "--certificate-path=certs",
        "--app=%(app_name)s", "--console-level=INFO",
        "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
        "--symbols-path=%(symbols_path)s",
        "--robocop=mochitest/robocop.ini"
    ],
    "xpcshell_options": [
        "--deviceIP=%(device_ip)s",
        "--xre-path=../hostutils/xre",
        "--manifest=xpcshell/tests/xpcshell_android.ini",
        "--build-info-json=xpcshell/mozinfo.json",
        "--testing-modules-dir=modules",
        "--local-lib-dir=../fennec",
        "--apk=../%(apk_name)s",
        "--no-logfiles",
        "--symbols-path=%(symbols_path)s"
    ],
    "jittest_options": [
        "bin/js",
        "--remote",
        "-j", "1",
        "--deviceTransport=sut",
        "--deviceIP=%(device_ip)s",
        "--localLib=../tests/bin",
        "--no-slow",
        "--no-progress",
        "--tinderbox",
        "--tbpl"
     ],
     "cppunittest_options": [
        "--symbols-path=%(symbols_path)s",
        "--xre-path=tests/bin",
        "--dm_trans=SUT",
        "--deviceIP=%(device_ip)s",
        "--localBinDir=../tests/bin",
        "--apk=%(apk_path)s",
        "--skip-manifest=../tests/cppunittests/android_cppunittest_manifest.txt"
     ],
}
