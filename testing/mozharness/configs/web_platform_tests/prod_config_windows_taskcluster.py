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
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        {
            'name': 'disable_screen_saver',
            'cmd': ['xset', 's', 'off', 's', 'reset'],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': False,
            'enabled': False
        },
        {
            'name': 'run mouse & screen adjustment script',
            'cmd': [
                sys.executable,
                os.path.join(os.getcwd(),
                    'mozharness', 'external_tools', 'mouse_and_screen_resolution.py'),
                '--configuration-file',
                os.path.join(os.getcwd(),
                    'mozharness', 'external_tools', 'machine-configuration.json')
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': True
        }
    ],

    "download_minidump_stackwalk": True,

    "per_test_category": "web-platform",
}
