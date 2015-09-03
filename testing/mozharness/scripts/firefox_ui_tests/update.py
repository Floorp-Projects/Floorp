#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""firefox_ui_updates.py

Author: Armen Zambrano G.
        Henrik Skupin
"""
import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.mozilla.testing.firefox_ui_tests import FirefoxUIUpdateTests


if __name__ == '__main__':
    myScript = FirefoxUIUpdateTests()
    myScript.run_and_exit()
