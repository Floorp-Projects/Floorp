# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import JavascriptException, NoSuchElementException

class TestSwitchFrameChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test_anonymous_content.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_switch(self):
        self.marionette.find_element("id", "testAnonymousContentBox")
        anon_browser_el = self.marionette.find_element("id", "browser")
        self.assertTrue("test_anonymous_content.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(anon_browser_el)
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.find_element("id", "testXulBox")
        self.assertRaises(NoSuchElementException, self.marionette.find_element, "id", "testAnonymousContentBox")
