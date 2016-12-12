# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


class TestClickChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.root_window = self.marionette.current_window_handle
        self.marionette.set_context("chrome")
        self.marionette.execute_script(
            "window.open('chrome://marionette/content/test.xul', 'foo', 'chrome,centerscreen')")
        self.marionette.switch_to_window("foo")
        self.assertNotEqual(self.root_window, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.root_window, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close()")
        self.marionette.switch_to_window(self.root_window)
        MarionetteTestCase.tearDown(self)

    def test_click(self):
        def checked():
            return self.marionette.execute_script(
                "return arguments[0].checked",
                script_args=[box])

        box = self.marionette.find_element(By.ID, "testBox")
        self.assertFalse(checked())
        box.click()
        self.assertTrue(checked())
