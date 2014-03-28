# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "jsreftest_options": [
        "--adbpath=%(adbpath)s", "--b2gpath=%(b2gpath)s", "--emulator=%(emulator)s",
        "--emulator-res=800x1000", "--logcat-dir=%(logcat_dir)s",
        "--remote-webserver=%(remote_webserver)s", "--ignore-window-size",
        "--xre-path=%(xre_path)s", "--symbols-path=%(symbols_path)s", "--busybox=%(busybox)s",
        "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
        "--extra-profile-file=jsreftest/tests/user.js",
        "%(test_manifest)s",
    ],

    "mochitest_options": [
        "--adbpath=%(adbpath)s", "--b2gpath=%(b2gpath)s", "--console-level=INFO",
        "--emulator=%(emulator)s", "--logcat-dir=%(logcat_dir)s",
        "--remote-webserver=%(remote_webserver)s", "%(test_manifest)s",
        "--xre-path=%(xre_path)s", "--symbols-path=%(symbols_path)s", "--busybox=%(busybox)s",
        "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
        "--quiet",
    ],

    "reftest_options": [
        "--adbpath=%(adbpath)s", "--b2gpath=%(b2gpath)s", "--emulator=%(emulator)s",
        "--emulator-res=800x1000", "--logcat-dir=%(logcat_dir)s",
        "--remote-webserver=%(remote_webserver)s", "--ignore-window-size",
        "--xre-path=%(xre_path)s", "--symbols-path=%(symbols_path)s", "--busybox=%(busybox)s",
        "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
        "%(test_manifest)s",
    ],

    "crashtest_options": [
        "--adbpath=%(adbpath)s", "--b2gpath=%(b2gpath)s", "--emulator=%(emulator)s",
        "--emulator-res=800x1000", "--logcat-dir=%(logcat_dir)s",
        "--remote-webserver=%(remote_webserver)s", "--ignore-window-size",
        "--xre-path=%(xre_path)s", "--symbols-path=%(symbols_path)s", "--busybox=%(busybox)s",
        "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
        "%(test_manifest)s",
    ],

    "xpcshell_options": [
        "--adbpath=%(adbpath)s", "--b2gpath=%(b2gpath)s", "--emulator=%(emulator)s",
        "--logcat-dir=%(logcat_dir)s", "--manifest=%(test_manifest)s", "--use-device-libs",
        "--testing-modules-dir=%(modules_dir)s", "--symbols-path=%(symbols_path)s",
        "--busybox=%(busybox)s", "--total-chunks=%(total_chunks)s", "--this-chunk=%(this_chunk)s",
    ],
}
