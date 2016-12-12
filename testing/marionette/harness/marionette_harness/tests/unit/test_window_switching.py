# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestWindowSwitching(WindowManagerMixin, MarionetteTestCase):

    def testJSWindowCreationAndSwitching(self):
        test_html = self.marionette.absolute_url("test_windows.html")
        self.marionette.navigate(test_html)

        def open_window_with_link():
            link = self.marionette.find_element(By.LINK_TEXT, "Open new window")
            link.click()

        new_window = self.open_window(trigger=open_window_with_link)
        self.marionette.switch_to_window(new_window)

        title = self.marionette.execute_script("return document.title")
        results_page = self.marionette.absolute_url("resultPage.html")
        self.assertEqual(self.marionette.get_url(), results_page)
        self.assertEqual(title, "We Arrive Here")

        # ensure navigate works in our current window
        other_page = self.marionette.absolute_url("test.html")
        self.marionette.navigate(other_page)

        # try to access its dom
        # since Bug 720714 stops us from checking DOMContentLoaded, we wait a bit
        Wait(self.marionette, timeout=30, ignored_exceptions=NoSuchElementException).until(
            lambda m: m.find_element(By.ID, 'mozLink'))

        self.assertEqual(new_window, self.marionette.current_chrome_window_handle)
        self.marionette.switch_to_window(self.start_window)
        self.assertEqual(self.start_window, self.marionette.current_chrome_window_handle)

    def tearDown(self):
        self.close_all_windows()
        super(TestWindowSwitching, self).tearDown()
