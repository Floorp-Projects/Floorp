# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By
from marionette_driver.errors import JavascriptException

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSwitchFrameChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSwitchFrameChrome, self).setUp()
        self.marionette.set_context("chrome")

        new_window = self.open_chrome_window("chrome://marionette/content/test.xhtml")
        self.marionette.switch_to_window(new_window)
        self.assertNotEqual(self.start_window, self.marionette.current_chrome_window_handle)

    def tearDown(self):
        self.close_all_windows()
        super(TestSwitchFrameChrome, self).tearDown()

    def test_switch_simple(self):
        self.assertIn("test.xhtml", self.marionette.get_url(), "Initial navigation has failed")
        self.marionette.switch_to_frame(0)
        self.assertIn("test.xhtml", self.marionette.get_url(), "Switching by index failed")
        self.marionette.find_element(By.ID, "testBox")
        self.marionette.switch_to_frame()
        self.assertIn("test.xhtml", self.marionette.get_url(), "Switching by null failed")
        iframe = self.marionette.find_element(By.ID, "iframe")
        self.marionette.switch_to_frame(iframe)
        self.assertIn("test.xhtml", self.marionette.get_url(), "Switching by element failed")
        self.marionette.find_element(By.ID, "testBox")

    def test_stack_trace(self):
        self.assertIn("test.xhtml", self.marionette.get_url(), "Initial navigation has failed")
        self.marionette.switch_to_frame(0)
        self.marionette.find_element(By.ID, "testBox")
        try:
            self.marionette.execute_async_script("foo();")
        except JavascriptException as e:
            self.assertIn("foo", str(e))
