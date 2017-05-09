# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import types

from marionette_driver import By, errors

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestWindowHandles(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestWindowHandles, self).setUp()

        self.empty_page = self.marionette.absolute_url("empty.html")
        self.test_page = self.marionette.absolute_url("windowHandles.html")
        self.marionette.navigate(self.test_page)

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        self.close_all_tabs()

        super(TestWindowHandles, self).tearDown()

    def assert_window_handles(self):
        try:
            self.assertIsInstance(self.marionette.current_chrome_window_handle, types.StringTypes)
            self.assertIsInstance(self.marionette.current_window_handle, types.StringTypes)
        except errors.NoSuchWindowException:
            pass

        for handle in self.marionette.chrome_window_handles:
            self.assertIsInstance(handle, types.StringTypes)

        for handle in self.marionette.window_handles:
            self.assertIsInstance(handle, types.StringTypes)

    def test_chrome_window_handles_with_scopes(self):
        # Open a browser and a non-browser (about window) chrome window
        self.open_window(
            trigger=lambda: self.marionette.execute_script("window.open();"))
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.chrome_window_handles), len(self.start_windows) + 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        self.open_window(
            trigger=lambda: self.marionette.find_element(By.ID, "aboutName").click())
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.chrome_window_handles), len(self.start_windows) + 2)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        chrome_window_handles_in_chrome_scope = self.marionette.chrome_window_handles
        window_handles_in_chrome_scope = self.marionette.window_handles

        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.chrome_window_handles,
                             chrome_window_handles_in_chrome_scope)
            self.assertEqual(self.marionette.window_handles,
                             window_handles_in_chrome_scope)

    def test_chrome_window_handles_after_opening_new_window(self):
        def open_with_link():
            with self.marionette.using_context("content"):
                link = self.marionette.find_element(By.ID, "new-window")
                link.click()

        # We open a new window but are actually interested in the new tab
        new_win = self.open_window(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.chrome_window_handles), len(self.start_windows) + 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        # Check that the new tab has the correct page loaded
        self.marionette.switch_to_window(new_win)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_chrome_window_handle, new_win)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

        # Ensure navigate works in our current window
        other_page = self.marionette.absolute_url("test.html")
        with self.marionette.using_context("content"):
            self.marionette.navigate(other_page)
            self.assertEqual(self.marionette.get_url(), other_page)

        # Close the opened window and carry on in our original tab.
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.chrome_window_handles), len(self.start_windows))

        self.marionette.switch_to_window(self.start_window)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

    def test_window_handles_after_opening_new_tab(self):
        def open_with_link():
            with self.marionette.using_context("content"):
                link = self.marionette.find_element(By.ID, "new-tab")
                link.click()

        new_tab = self.open_tab(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

        # Ensure navigate works in our current tab
        other_page = self.marionette.absolute_url("test.html")
        with self.marionette.using_context("content"):
            self.marionette.navigate(other_page)
            self.assertEqual(self.marionette.get_url(), other_page)

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_opening_new_window(self):
        def open_with_link():
            with self.marionette.using_context("content"):
                link = self.marionette.find_element(By.ID, "new-window")
                link.click()

        # We open a new window but are actually interested in the new tab
        new_tab = self.open_tab(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        # Check that the new tab has the correct page loaded
        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

        # Ensure navigate works in our current window
        other_page = self.marionette.absolute_url("test.html")
        with self.marionette.using_context("content"):
            self.marionette.navigate(other_page)
            self.assertEqual(self.marionette.get_url(), other_page)

        # Close the opened window and carry on in our original tab.
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

    def test_window_handles_after_closing_original_tab(self):
        def open_with_link():
            with self.marionette.using_context("content"):
                link = self.marionette.find_element(By.ID, "new-tab")
                link.click()

        new_tab = self.open_tab(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

    def test_window_handles_no_switch(self):
        """Regression test for bug 1294456.
        This test is testing the case where Marionette attempts to send a
        command to a window handle when the browser has opened and selected
        a new tab. Before bug 1294456 landed, the Marionette driver was getting
        confused about which window handle the client cared about, and assumed
        it was the window handle for the newly opened and selected tab.

        This caused Marionette to think that the browser needed to do a remoteness
        flip in the e10s case, since the tab opened by menu_newNavigatorTab is
        about:newtab (which is currently non-remote). This meant that commands
        sent to what should have been the original window handle would be
        queued and never sent, since the remoteness flip in the new tab was
        never going to happen.
        """
        def open_with_menu():
            menu_new_tab = self.marionette.find_element(By.ID, 'menu_newNavigatorTab')
            menu_new_tab.click()

        new_tab = self.open_tab(trigger=open_with_menu)
        self.assert_window_handles()

        # We still have the default tab set as our window handle. This
        # get_url command should be sent immediately, and not be forever-queued.
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_closing_last_window(self):
        self.close_all_windows()
        self.assertEqual(self.marionette.close_chrome_window(), [])
