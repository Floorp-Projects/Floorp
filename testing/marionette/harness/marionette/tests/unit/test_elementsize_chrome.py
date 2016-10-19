# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.by import By


class TestElementSizeChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_chrome_window_handle
        self.marionette.execute_script(
            "window.open('chrome://marionette/content/test2.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_chrome_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_chrome_window_handle)
        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def testShouldReturnTheSizeOfAnInput(self):
        wins = self.marionette.chrome_window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        shrinko = self.marionette.find_element(By.ID, 'textInput')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)
