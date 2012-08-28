# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestText(MarionetteTestCase):
    def test_getText(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("id", "mozLink")
        self.assertEqual("Click me!", l.text)

    def test_clearText(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myInput")
        self.assertEqual("asdf", self.marionette.execute_script("return arguments[0].value;", [l]))
        l.clear()
        self.assertEqual("", self.marionette.execute_script("return arguments[0].value;", [l]))

    def test_sendKeys(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("name", "myInput")
        self.assertEqual("asdf", self.marionette.execute_script("return arguments[0].value;", [l]))
        l.send_keys("o")
        self.assertEqual("asdfo", self.marionette.execute_script("return arguments[0].value;", [l]))

class TestTextChrome(MarionetteTestCase):
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

    def test_getText(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text)

    def test_clearText(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text)
        box.clear()
        self.assertEqual("", box.text)

    def test_sendKeys(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text)
        box.send_keys("at")
        self.assertEqual("attest", box.text)
