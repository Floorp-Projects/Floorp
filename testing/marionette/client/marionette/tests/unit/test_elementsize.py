# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class TestElementSize(MarionetteTestCase):
    def testShouldReturnTheSizeOfALink(self):
        test_html = self.marionette.absolute_url("testSize.html")
        self.marionette.navigate(test_html)
        shrinko = self.marionette.find_element('id', 'linkId')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)

class TestElementSizeChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test2.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def testShouldReturnTheSizeOfAnInput(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        shrinko = self.marionette.find_element('id', 'textInput')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)

