# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase, WindowManagerMixin
from marionette_driver import By


class TestSwitchWindow(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSwitchWindow, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        super(TestSwitchWindow, self).tearDown()

    def test_windows(self):
        def open_browser_with_js():
            self.marionette.execute_script(" window.open(); ")

        new_window = self.open_window(trigger=open_browser_with_js)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        # switch to the other window
        self.marionette.switch_to_window(new_window)
        self.assertEqual(self.marionette.current_chrome_window_handle, new_window)
        self.assertNotEqual(self.marionette.current_chrome_window_handle, self.start_window)

        # switch back and close original window
        self.marionette.switch_to_window(self.start_window)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.marionette.close_chrome_window()
        self.assertNotIn(self.start_window, self.marionette.chrome_window_handles)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.assertEqual(len(self.marionette.chrome_window_handles), len(self.start_windows))

    def test_should_load_and_close_a_window(self):
        def open_window_with_link():
            test_html = self.marionette.absolute_url("test_windows.html")
            with self.marionette.using_context("content"):
                self.marionette.navigate(test_html)
                self.marionette.find_element(By.LINK_TEXT, "Open new window").click()

        new_window = self.open_window(trigger=open_window_with_link)
        self.marionette.switch_to_window(new_window)
        self.assertEqual(self.marionette.current_chrome_window_handle, new_window)
        self.assertEqual(len(self.marionette.chrome_window_handles), 2)

        with self.marionette.using_context('content'):
            self.assertEqual(self.marionette.title, "We Arrive Here")

        # Let's close and check
        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)
        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
