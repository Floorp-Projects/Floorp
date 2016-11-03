# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from marionette import MarionetteTestCase, WindowManagerMixin
from marionette_driver.by import By


class TestElementSizeChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestElementSizeChrome, self).setUp()

        self.marionette.set_context("chrome")

        def open_window_with_js():
            self.marionette.execute_script("""
              window.open('chrome://marionette/content/test2.xul',
                          'foo', 'chrome,centerscreen');
            """)

        new_window = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(new_window)

    def tearDown(self):
        self.close_all_windows()
        super(TestElementSizeChrome, self).tearDown()

    def testShouldReturnTheSizeOfAnInput(self):
        shrinko = self.marionette.find_element(By.ID, 'textInput')
        size = shrinko.rect
        self.assertTrue(size['width'] > 0)
        self.assertTrue(size['height'] > 0)
