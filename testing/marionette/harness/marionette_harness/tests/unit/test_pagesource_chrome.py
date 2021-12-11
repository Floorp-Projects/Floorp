# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestPageSourceChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestPageSourceChrome, self).setUp()
        self.marionette.set_context("chrome")

        new_window = self.open_chrome_window(
            "chrome://remote/content/marionette/test.xhtml"
        )
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()
        super(TestPageSourceChrome, self).tearDown()

    def testShouldReturnXULDetails(self):
        source = self.marionette.page_source
        self.assertTrue(
            '<input xmlns="http://www.w3.org/1999/xhtml" id="textInput"' in source
        )
