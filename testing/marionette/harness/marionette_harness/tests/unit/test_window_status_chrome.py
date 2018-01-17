# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from test_window_status_content import TestNoSuchWindowContent


class TestNoSuchWindowChrome(TestNoSuchWindowContent):

    def setUp(self):
        super(TestNoSuchWindowChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestNoSuchWindowChrome, self).tearDown()
