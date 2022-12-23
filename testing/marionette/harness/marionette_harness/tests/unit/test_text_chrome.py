# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestTextChrome(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestTextChrome, self).setUp()
        win = self.open_chrome_window("chrome://remote/content/marionette/test.xhtml")
        self.marionette.switch_to_window(win)

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestTextChrome, self).tearDown()

    def test_get_text(self):
        elem = self.marionette.find_element(By.ID, "testBox")
        self.assertEqual(elem.text, "box")

    def test_clear_text(self):
        input = self.marionette.find_element(By.ID, "textInput3")
        self.assertEqual(
            "test",
            self.marionette.execute_script("return arguments[0].value;", [input]),
        )
        input.clear()
        self.assertEqual(
            "", self.marionette.execute_script("return arguments[0].value;", [input])
        )
