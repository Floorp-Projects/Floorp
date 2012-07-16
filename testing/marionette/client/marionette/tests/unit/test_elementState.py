# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestState(MarionetteTestCase):
    def test_isEnabled(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myCheckBox")
        self.assertTrue(l.enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.enabled())

    def test_isDisplayed(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myCheckBox")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())

class TestStateChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test.xul', '_blank', 'chrome,centerscreen');")

    def tearDown(self):
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_isEnabled(self):
        l = self.marionette.find_element("id", "textInput")
        self.assertTrue(l.enabled())
        self.marionette.execute_script("arguments[0].disabled = true;", [l])
        self.assertFalse(l.enabled())
        self.marionette.execute_script("arguments[0].disabled = false;", [l])

    def test_isDisplayed(self):
        l = self.marionette.find_element("id", "textInput")
        self.assertTrue(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.is_displayed())
        self.marionette.execute_script("arguments[0].hidden = false;", [l])

