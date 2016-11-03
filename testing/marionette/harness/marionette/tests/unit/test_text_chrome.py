# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase, WindowManagerMixin
from marionette_driver.by import By


''' Disabled in bug 896043 and when working on Chrome code re-enable for bug 896046
class TestTextChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestTextChrome, self).setUp()
        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test.xul',
                          'foo', 'chrome,centerscreen');
            """)

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()
        super(TestTextChrome, self).tearDown()

    def test_getText(self):
        box = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual("test", box.text)

    def test_clearText(self):
        box = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual("test", box.text)
        box.clear()
        self.assertEqual("", box.text)

    def test_sendKeys(self):
        box = self.marionette.find_element(By.ID, "textInput")
        self.assertEqual("test", box.text)
        box.send_keys("at")
        self.assertEqual("attest", box.text)
'''
