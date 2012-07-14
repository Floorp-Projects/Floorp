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

    def testClickingALinkMadeUpOfNumbersIsHandledCorrectly(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element("link text", "333333").click()
        self.assertEqual(self.marionette.title, "XHTML Test Page")

class TestClickChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test.xul', '_blank', 'chrome,centerscreen');")

    def tearDown(self):
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_click(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        box = self.marionette.find_element("id", "testBox")
        self.assertFalse(self.marionette.execute_script("return arguments[0].checked;", [box]))
        box.click()
        self.assertTrue(self.marionette.execute_script("return arguments[0].checked;", [box]))
