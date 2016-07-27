# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import JavascriptException


class TestSwitchFrameChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_switch_simple(self):
        self.assertIn("test.xul", self.marionette.get_url(), "Initial navigation has failed")
        self.marionette.switch_to_frame(0)
        self.assertIn("test2.xul", self.marionette.get_url(),"Switching by index failed")
        self.marionette.switch_to_frame()
        self.assertEqual(None, self.marionette.get_active_frame(), "Switiching by null failed")
        self.assertIn("test.xul", self.marionette.get_url(), "Switching by null failed")
        self.marionette.switch_to_frame("iframe")
        self.assertIn("test2.xul", self.marionette.get_url(), "Switching by name failed")
        self.marionette.switch_to_frame()
        self.assertIn("test.xul", self.marionette.get_url(), "Switching by null failed")
        self.marionette.switch_to_frame("iframename")
        self.assertIn("test2.xul", self.marionette.get_url(), "Switching by name failed")
        iframe_element = self.marionette.get_active_frame()
        self.marionette.switch_to_frame()
        self.assertIn("test.xul", self.marionette.get_url(), "Switching by null failed")
        self.marionette.switch_to_frame(iframe_element)
        self.assertIn("test2.xul", self.marionette.get_url(), "Switching by element failed")

    def test_stack_trace(self):
        self.assertIn("test.xul", self.marionette.get_url(), "Initial navigation has failed")
        self.marionette.switch_to_frame(0)
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, "foo();")
        try:
            self.marionette.execute_async_script("foo();")
        except JavascriptException as e:
            self.assertIn("foo", e.message)
