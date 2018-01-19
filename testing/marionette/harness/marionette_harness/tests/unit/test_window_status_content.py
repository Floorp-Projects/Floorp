# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By
from marionette_driver.errors import NoSuchWindowException

from marionette_harness import MarionetteTestCase, WindowManagerMixin, skip_if_mobile


class TestNoSuchWindowContent(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNoSuchWindowContent, self).setUp()

        self.test_page = self.marionette.absolute_url("windowHandles.html")
        with self.marionette.using_context("content"):
            self.marionette.navigate(self.test_page)

    def tearDown(self):
        self.close_all_windows()
        super(TestNoSuchWindowContent, self).tearDown()

    def open_tab_in_foreground(self):
        with self.marionette.using_context("content"):
            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_closed_chrome_window(self):

        def open_with_link():
            with self.marionette.using_context("content"):
                test_page = self.marionette.absolute_url("windowHandles.html")
                self.marionette.navigate(test_page)
                self.marionette.find_element(By.ID, "new-window").click()

        win = self.open_window(open_with_link)
        self.marionette.switch_to_window(win)
        self.marionette.close_chrome_window()

        # When closing a browser window both handles are not available
        for context in ("chrome", "content"):
            with self.marionette.using_context(context):
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_chrome_window_handle
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_window_handle

        self.marionette.switch_to_window(self.start_window)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(win)

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_closed_chrome_window_while_in_frame(self):

        def open_window_with_js():
            with self.marionette.using_context("chrome"):
                self.marionette.execute_script("""
                  window.open('chrome://marionette/content/test.xul',
                              'foo', 'chrome,centerscreen');
                """)

        win = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(win)
        with self.marionette.using_context("chrome"):
            self.marionette.switch_to_frame("iframe")
        self.marionette.close_chrome_window()

        with self.assertRaises(NoSuchWindowException):
            self.marionette.current_window_handle
        with self.assertRaises(NoSuchWindowException):
            self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_window)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(win)

    def test_closed_tab(self):
        with self.marionette.using_context("content"):
            tab = self.open_tab(self.open_tab_in_foreground)
            self.marionette.switch_to_window(tab)
            self.marionette.close()

        # Check that only the content window is not available in both contexts
        for context in ("chrome", "content"):
            with self.marionette.using_context(context):
                with self.assertRaises(NoSuchWindowException):
                    self.marionette.current_window_handle
                self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_tab)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(tab)

    def test_closed_tab_while_in_frame(self):
        with self.marionette.using_context("content"):
            tab = self.open_tab(self.open_tab_in_foreground)
            self.marionette.switch_to_window(tab)
            self.marionette.navigate(self.marionette.absolute_url("test_iframe.html"))
            frame = self.marionette.find_element(By.ID, "test_iframe")
            self.marionette.switch_to_frame(frame)
            self.marionette.close()

            with self.assertRaises(NoSuchWindowException):
                self.marionette.current_window_handle
            self.marionette.current_chrome_window_handle

        self.marionette.switch_to_window(self.start_tab)

        with self.assertRaises(NoSuchWindowException):
            self.marionette.switch_to_window(tab)
