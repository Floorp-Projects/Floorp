# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "suite_definitions": {
        "mochitest": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": ["--autorun", "--close-when-done", "--dm_trans=sut",
                "--console-level=INFO", "--app=%(app)s", "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s", "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s", "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s", "--symbols-path=%(symbols_path)s",
                "--quiet", "--log-raw=%(raw_log_file)s",
                # Bug 1064002 - Land once mozharness changes land
                #"--total-chunks=16",
                #"--run-only-tests=android23.json",
            ],
        },
        # Bug 1064002 - Not yet in use
        "mochitest-gl": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": ["--autorun", "--close-when-done", "--dm_trans=sut",
                "--console-level=INFO", "--app=%(app)s", "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s", "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s", "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s", "--symbols-path=%(symbols_path)s",
                "--quiet", "--log-raw=%(raw_log_file)s",
                "--total-chunks=2",
                "--test-manifest=gl.json",
            ],
        },
        # Bug 1064002 - Not yet in use
        "robocop": {
            "run_filename": "runtestsremote.py",
            "testsdir": "mochitest",
            "options": ["--autorun", "--close-when-done", "--dm_trans=sut",
                "--console-level=INFO", "--app=%(app)s", "--remote-webserver=%(remote_webserver)s",
                "--xre-path=%(xre_path)s", "--utility-path=%(utility_path)s",
                "--deviceIP=%(device_ip)s", "--devicePort=%(device_port)s",
                "--http-port=%(http_port)s", "--ssl-port=%(ssl_port)s",
                "--certificate-path=%(certs_path)s", "--symbols-path=%(symbols_path)s",
                "--quiet", "--log-raw=%(raw_log_file)s",
                "--total-chunks=4",
                "--robocop-path=../..",
                "--robocop-ids=fennec_ids.txt",
                "--robocop=robocop.ini",
            ],
        },
        "reftest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [ "--app=%(app)s", "--ignore-window-size",
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s", "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s", "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s", "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s", "--httpd-path", "reftest/components",
                "--symbols-path=%(symbols_path)s",
                # Bug 1064002 - Land once mozharness changes land
                #"--total-chunks=16",
                #"tests/layout/reftests/reftest.list",
            ],
        },
        # Bug 1064002 - Not yet in use
        "crashtest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [ "--app=%(app)s", "--ignore-window-size",
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s", "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s", "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s", "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s", "--httpd-path", "reftest/components",
                "--symbols-path=%(symbols_path)s",
                "--total-chunks=2",
                "tests/testing/crashtest/crashtests.list",
            ],
        },
        # Bug 1064002 - Not yet in use
        "jsreftest": {
            "run_filename": "remotereftest.py",
            "testsdir": "reftest",
            "options": [ "--app=%(app)s", "--ignore-window-size",
                "--bootstrap",
                "--remote-webserver=%(remote_webserver)s", "--xre-path=%(xre_path)s",
                "--utility-path=%(utility_path)s", "--deviceIP=%(device_ip)s",
                "--devicePort=%(device_port)s", "--http-port=%(http_port)s",
                "--ssl-port=%(ssl_port)s", "--httpd-path", "reftest/components",
                "--symbols-path=%(symbols_path)s",
                "../jsreftest/tests/jstests.list",
                "--total-chunks=6",
                "--extra-profile-file=jsreftest/tests/user.js",
            ],
        },
        "xpcshell": {
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell",
            "options": ["--deviceIP=%(device_ip)s", "--devicePort=%(device_port)s",
                "--xre-path=%(xre_path)s", "--testing-modules-dir=%(modules_dir)s",
                "--apk=%(installer_path)s", "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--manifest=tests/xpcshell.ini",
                # Bug 1064002 - Land once mozharness changes land
                #"--log-raw=%(raw_log_file)s",
                #"--total-chunks=3",
            ],
        },
    }, # end suite_definitions
}
