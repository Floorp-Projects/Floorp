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
        self.assertEqual("Click me!", l.text())

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

    def test_getText(self):
        wins = self.marionette.get_windows()
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text())

    def test_clearText(self):
        wins = self.marionette.get_windows()
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text())
        box.clear()
        self.assertEqual("", box.text())

    def test_sendKeys(self):
        wins = self.marionette.get_windows()
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "textInput")
        self.assertEqual("test", box.text())
        box.send_keys("at")
        self.assertEqual("attest", box.text())
