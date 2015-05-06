# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "reftest_options": [
        "--appname=%(binary_path)s", "--utility-path=tests/bin",
        "--extra-profile-file=tests/bin/plugins", "--symbols-path=%(symbols_path)s"
    ],
    "mochitest_options": [
        "--appname=%(binary_path)s", "--utility-path=tests/bin",
        "--extra-profile-file=tests/bin/plugins", "--symbols-path=%(symbols_path)s",
        "--certificate-path=tests/certs", "--setpref=webgl.force-enabled=true",
        "--quiet", "--log-raw=%(raw_log_file)s"
    ],
    "webapprt_options": [
        "--app=%(app_path)s", "--utility-path=tests/bin",
        "--extra-profile-file=tests/bin/plugins", "--symbols-path=%(symbols_path)s",
        "--certificate-path=tests/certs", "--autorun", "--close-when-done",
        "--console-level=INFO", "--testing-modules-dir=tests/modules",
        "--quiet"
    ],
    "xpcshell_options": [
        "--symbols-path=%(symbols_path)s",
        "--test-plugin-path=%(test_plugin_path)s"
    ],
    "cppunittest_options": [
        "--symbols-path=%(symbols_path)s",
        "--xre-path=%(abs_app_dir)s"
    ],
    "jittest_options": [
        "tests/bin/js",
        "--no-slow",
        "--no-progress",
        "--tinderbox",
        "--tbpl"
    ],
    "mozbase_options": [
        "-b", "%(binary_path)s"
    ],
}
