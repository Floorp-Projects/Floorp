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

    def tearDown(self):
        self.close_all_tabs()
        super(TestNoSuchWindowContent, self).tearDown()

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
            tab = self.open_tab()
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
            tab = self.open_tab()
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


class TestNoSuchWindowChrome(TestNoSuchWindowContent):

    def setUp(self):
        super(TestNoSuchWindowChrome, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        super(TestNoSuchWindowChrome, self).tearDown()


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
