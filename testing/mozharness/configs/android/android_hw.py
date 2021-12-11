# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# mozharness configuration for Android hardware unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "exes": {},
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s",
    },
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "verify-device",
        "install",
        "run-tests",
    ],
    "tooltool_cache": "/builds/tooltool_cache",
    # from android_common.py
    "download_tooltool": True,
    "xpcshell_extra": "--remoteTestRoot=/data/local/tmp/test_root",
}
