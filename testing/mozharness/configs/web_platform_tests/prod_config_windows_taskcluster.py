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
        'virtualenv': [
            sys.executable,
            os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages', 'virtualenv.py')
        ],
        'mozinstall': ['build/venv/scripts/python', 'build/venv/scripts/mozinstall-script.py'],
        'tooltool.py': [sys.executable, os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')],
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "proxxy": {},
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],

    "pip_index": False,

    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],

    "blob_uploader_auth_file" : 'C:/builds/oauth.txt',

    "download_minidump_stackwalk": True,
}
