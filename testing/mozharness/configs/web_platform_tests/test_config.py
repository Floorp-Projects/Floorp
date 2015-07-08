# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

config = {
    # test harness options are located in the gecko tree
    "in_tree_config": "config/mozharness/web_platform_tests_config.py",

    "options": [],

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'pull',
        'install',
        'run-tests',
    ],

    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],

    "pip_index": False,
}
