# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestTitleChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestTitleChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestTitleChrome, self).tearDown()

    def test_get_title_xhtml(self):
        win = self.open_chrome_window(
            "chrome://remote/content/marionette/test_no_xul.xhtml"
        )
        self.marionette.switch_to_window(win)

        expected_title = self.marionette.execute_script("return window.document.title;")
        self.assertEqual(self.marionette.title, expected_title)

    def test_get_title_xul(self):
        win = self.open_chrome_window("chrome://remote/content/marionette/test.xhtml")
        self.marionette.switch_to_window(win)

        expected_title = self.marionette.execute_script(
            """
            return window.document.documentElement.getAttribute('title');
        """
        )
        self.assertEqual(self.marionette.title, expected_title)
