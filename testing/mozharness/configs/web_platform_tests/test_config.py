# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

config = {
    "options": [
        "--prefs-root=%(test_path)s/prefs",
        "--config=%(test_path)s/wptrunner.ini",
        "--ca-cert-path=%(test_path)s/tests/tools/certs/cacert.pem",
        "--host-key-path=%(test_path)s/tests/tools/certs/web-platform.test.key",
        "--host-cert-path=%(test_path)s/tests/tools/certs/web-platform.test.pem",
        "--certutil-binary=%(test_install_path)s/bin/certutil",
    ],
    "default_actions": [
        "clobber",
        "download-and-extract",
        "create-virtualenv",
        "pull",
        "install",
        "run-tests",
    ],
}
