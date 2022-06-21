# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestElementSizeChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestElementSizeChrome, self).setUp()

        self.marionette.set_context("chrome")

        new_window = self.open_chrome_window(
            "chrome://remote/content/marionette/test.xhtml"
        )
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()
        super(TestElementSizeChrome, self).tearDown()

    def test_payload(self):
        rect = self.marionette.find_element(By.ID, "textInput").rect
        self.assertTrue(rect["x"] > 0)
        self.assertTrue(rect["y"] > 0)
        self.assertTrue(rect["width"] > 0)
        self.assertTrue(rect["height"] > 0)
