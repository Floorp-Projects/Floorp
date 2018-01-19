# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestWindowTypeChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestWindowTypeChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestWindowTypeChrome, self).tearDown()

    def test_get_window_type(self):

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test.xul',
                          'foo', 'chrome,centerscreen');
            """)

        win = self.open_window(open_window_with_js)
        self.marionette.switch_to_window(win)

        window_type = self.marionette.execute_script(
            "return window.document.documentElement.getAttribute('windowtype');")
        self.assertEqual(window_type, self.marionette.get_window_type())
