# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import types

from marionette_driver import By, errors, Wait

from marionette_harness import MarionetteTestCase, skip_if_mobile, WindowManagerMixin


class TestWindowHandles(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestWindowHandles, self).setUp()

        self.empty_page = self.marionette.absolute_url("empty.html")
        self.test_page = self.marionette.absolute_url("windowHandles.html")
        self.marionette.navigate(self.test_page)

    def tearDown(self):
        self.close_all_tabs()

        super(TestWindowHandles, self).tearDown()

    def assert_window_handles(self):
        try:
            self.assertIsInstance(self.marionette.current_window_handle, types.StringTypes)
        except errors.NoSuchWindowException:
            pass

        for handle in self.marionette.window_handles:
            self.assertIsInstance(handle, types.StringTypes)

    def test_window_handles_after_opening_new_tab(self):
        def open_with_link():
            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

        new_tab = self.open_tab(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == self.empty_page,
            message="{} did not load after opening a new tab".format(self.empty_page))

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_opening_new_browser_window(self):
        def open_with_link():
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
        Wait(self.marionette, self.marionette.timeout.page_load).until(
            lambda _: self.marionette.get_url() == self.empty_page,
            message="The expected page '{}' has not been loaded".format(self.empty_page))

        # Ensure navigate works in our current window
        other_page = self.marionette.absolute_url("test.html")
        self.marionette.navigate(other_page)
        self.assertEqual(self.marionette.get_url(), other_page)

        # Close the opened window and carry on in our original tab.
        self.marionette.close()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.marionette.get_url(), self.test_page)

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_window_handles_after_opening_new_non_browser_window(self):
        def open_with_link():
            self.marionette.navigate(self.marionette.absolute_url("blob_download.html"))
            link = self.marionette.find_element(By.ID, "blob-download")
            link.click()

        new_win = self.open_window(trigger=open_with_link)
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_win)
        self.assert_window_handles()

        # Check that the opened window is not accessible via window handles
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.current_window_handle
        with self.assertRaises(errors.NoSuchWindowException):
            self.marionette.close()

        # Close the opened window and carry on in our original tab.
        self.marionette.close_chrome_window()
        self.assert_window_handles()
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs))

        self.marionette.switch_to_window(self.start_tab)
        self.assert_window_handles()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

    def test_window_handles_after_closing_original_tab(self):
        def open_with_link():
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
        Wait(self.marionette, self.marionette.timeout.page_load).until(
            lambda _: self.marionette.get_url() == self.empty_page,
            message="The expected page '{}' has not been loaded".format(self.empty_page))

    def test_window_handles_after_closing_last_tab(self):
        self.close_all_tabs()
        self.assertEqual(self.marionette.close(), [])
