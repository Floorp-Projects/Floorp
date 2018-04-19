# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

# This is a template config file for web-platform-tests test.

import os
import sys

config = {
    "options": [
        "--prefs-root=%(test_path)s/prefs",
        "--processes=1",
        "--config=%(test_path)s/wptrunner.ini",
        "--ca-cert-path=%(test_path)s/certs/cacert.pem",
        "--host-key-path=%(test_path)s/certs/web-platform.test.key",
        "--host-cert-path=%(test_path)s/certs/web-platform.test.pem",
        "--certutil-binary=%(test_install_path)s/bin/certutil",
    ],

    "exes": {
        'python': sys.executable,
        'hg': 'c:/mozilla-build/hg/hg',
    },

    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],

    "pip_index": False,

    "buildbot_json_path": "buildprops.json",

    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],

    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),

    "download_minidump_stackwalk": True,

    "per_test_category": "web-platform",
}
