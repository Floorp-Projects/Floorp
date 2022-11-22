# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import mozpack.path as mozpath

setup_called = False


def setup():
    """Add the directory of the targeted python modules to the python sys.path"""

    global setup_called
    if setup_called:
        return
    setup_called = True

    OUR_DIR = mozpath.abspath(mozpath.dirname(__file__))
    TARGET_MOD_DIR = mozpath.normpath(mozpath.join(OUR_DIR, ".."))
    sys.path.append(TARGET_MOD_DIR)
