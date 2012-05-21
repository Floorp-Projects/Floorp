# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestClick(MarionetteTestCase):
    def test_click(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element("id", "mozLink")
        link.click()
        self.assertEqual("Clicked", self.marionette.execute_script("return document.getElementById('mozLink').innerHTML;"))

class TestClickChrome(MarionetteTestCase):
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

    def test_click(self):
        wins = self.marionette.get_windows()
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "testBox")
        self.assertFalse(self.marionette.execute_script("return arguments[0].checked;", [box]))
        box.click()
        self.assertTrue(self.marionette.execute_script("return arguments[0].checked;", [box]))
