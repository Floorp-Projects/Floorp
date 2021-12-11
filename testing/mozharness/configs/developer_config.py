# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This config file can be appended to any other mozharness job
running under treeherder. The purpose of this config is to
override values that are specific to Release Engineering machines
that can reach specific hosts within their network.
In other words, this config allows you to run any job
outside of the Release Engineering network

Using this config file should be accompanied with using
--test-url and --installer-url where appropiate
"""

from __future__ import absolute_import
import os

LOCAL_WORKDIR = os.path.expanduser("~/.mozilla/releng")

config = {
    # Developer mode values
    "developer_mode": True,
    "local_workdir": LOCAL_WORKDIR,
    "replace_urls": [
        ("http://pvtbuilds.pvt.build", "https://pvtbuilds"),
    ],
    # General local variable overwrite
    "exes": {
        "gittool.py": os.path.join(LOCAL_WORKDIR, "gittool.py"),
    },
    # Talos related
    "python_webserver": True,
    "virtualenv_path": "%s/build/venv" % os.getcwd(),
    "preflight_run_cmd_suites": [],
    "postflight_run_cmd_suites": [],
    # Tooltool related
    "tooltool_cache": os.path.join(LOCAL_WORKDIR, "builds/tooltool_cache"),
    "tooltool_cache_path": os.path.join(LOCAL_WORKDIR, "builds/tooltool_cache"),
    # VCS tools
    "gittool.py": "http://hg.mozilla.org/build/puppet/raw-file/faaf5abd792e/modules/packages/files/gittool.py",
    # Android related
    "host_utils_url": "https://tooltool.mozilla-releng.net/sha512/372c89f9dccaf5ee3b9d35fd1cfeb089e1e5db3ff1c04e35aa3adc8800bc61a2ae10e321f37ae7bab20b56e60941f91bb003bcb22035902a73d70872e7bd3282",
}
