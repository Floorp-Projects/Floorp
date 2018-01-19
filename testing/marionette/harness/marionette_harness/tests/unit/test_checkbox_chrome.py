# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSelectedChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSelectedChrome, self).setUp()

        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test.xul',
                          '_blank', 'chrome,centerscreen');
            """)

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        try:
            self.close_all_windows()
        finally:
            super(TestSelectedChrome, self).tearDown()

    def test_selected(self):
        box = self.marionette.find_element(By.ID, "testBox")
        self.assertFalse(box.is_selected())
        self.assertFalse(self.marionette.execute_script("arguments[0].checked = true;", [box]))
        self.assertTrue(box.is_selected())
