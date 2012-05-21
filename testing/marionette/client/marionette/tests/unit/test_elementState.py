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
        self.assertTrue(l.displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.displayed())

class TestStateChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.get_window()
        #need to get the file:// path for xul
        unit = os.path.abspath(os.path.join(os.path.realpath(__file__), os.path.pardir))
        tests = os.path.abspath(os.path.join(unit, os.path.pardir))
        mpath = os.path.abspath(os.path.join(tests, os.path.pardir))
        xul = "file://" + os.path.join(mpath, "www", "test.xul")
        self.marionette.execute_script("window.open('" + xul +"', '_blank', 'chrome,centerscreen');")

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
        self.assertTrue(l.displayed())
        self.marionette.execute_script("arguments[0].hidden = true;", [l])
        self.assertFalse(l.displayed())
        self.marionette.execute_script("arguments[0].hidden = false;", [l])

