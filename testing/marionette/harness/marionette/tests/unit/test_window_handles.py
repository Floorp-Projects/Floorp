# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.keys import Keys
from marionette_driver.by import By


class TestWindowHandles(MarionetteTestCase):

    def test_new_tab_window_handles(self):

        keys = []
        if self.marionette.session_capabilities['platformName'] == 'Darwin':
            keys.append(Keys.META)
        else:
            keys.append(Keys.CONTROL)
        keys.append('t')

        origin_win = self.marionette.current_window_handle

        with self.marionette.using_context("chrome"):
            main_win = self.marionette.find_element(By.ID, "main-window")
            main_win.send_keys(*keys)

        self.wait_for_condition(lambda mn: len(mn.window_handles) == 2)
        handles = self.marionette.window_handles
        handles.remove(origin_win)
        new_tab = handles.pop()
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:newtab")
        self.marionette.close()

        self.marionette.switch_to_window(origin_win)
        self.assertEqual(self.marionette.get_url(), "about:blank")

    def test_link_opened_tab_window_handles(self):
        tab_testpage = self.marionette.absolute_url("windowHandles.html")
        self.marionette.navigate(tab_testpage)
        start_win = self.marionette.current_window_handle
        link = self.marionette.find_element(By.ID, "new-tab")
        link.click()
        self.wait_for_condition(lambda mn: len(mn.window_handles) == 2)

        handles = self.marionette.window_handles
        handles.remove(start_win)
        dest_win = handles.pop()

        self.marionette.switch_to_window(dest_win)
        self.assertEqual(self.marionette.get_url(), "about:blank")
        self.assertEqual(self.marionette.title, "")

        self.marionette.switch_to_window(start_win)

        self.assertIn('windowHandles.html', self.marionette.get_url())
        self.assertEqual(self.marionette.title, "Marionette New Tab Link")

        self.marionette.close()
        self.marionette.switch_to_window(dest_win)
        self.assertEqual(self.marionette.get_url(), "about:blank")

    def test_chrome_windows(self):
        opener_page = self.marionette.absolute_url("windowHandles.html")

        self.marionette.navigate(opener_page)

        # Window handles don't persist in cases of remoteness change.
        start_tab = self.marionette.current_window_handle

        self.marionette.find_element(By.ID, "new-window").click()

        self.assertEqual(len(self.marionette.window_handles), 2)
        self.assertEqual(len(self.marionette.chrome_window_handles), 2)
        windows = self.marionette.window_handles
        windows.remove(start_tab)
        dest_tab = windows.pop()
        self.marionette.switch_to_window(dest_tab)

        self.marionette.navigate(opener_page)
        new_tab_link = self.marionette.find_element(By.ID, "new-tab")

        for i in range(3):
            new_tab_link.click()
            self.marionette.switch_to_window(dest_tab)
            self.wait_for_condition(lambda mn: len(mn.window_handles) == i + 3)
            self.assertEqual(len(self.marionette.chrome_window_handles), 2)

        self.marionette.close_chrome_window()
        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(len(self.marionette.window_handles), 1)
        self.marionette.switch_to_window(start_tab)

    def test_chrome_window_handles_with_scopes(self):
        start_tab = self.marionette.current_window_handle
        start_chrome_window = self.marionette.current_chrome_window_handle
        start_chrome_windows = self.marionette.chrome_window_handles

        # Ensure that we work in chrome scope so we don't have any limitations
        with self.marionette.using_context("chrome"):
            # Open a browser and a non-browser (about window) chrome window
            self.marionette.execute_script("window.open()")
            self.marionette.find_element(By.ID, "aboutName").click()

            self.wait_for_condition(lambda mn: len(mn.chrome_window_handles) ==
                                    len(start_chrome_windows) + 2)

            handles_in_chrome_scope = self.marionette.chrome_window_handles
            with self.marionette.using_context("content"):
                self.assertEqual(self.marionette.chrome_window_handles,
                                 handles_in_chrome_scope)

        for handle in handles_in_chrome_scope:
            if handle != start_chrome_window:
                self.marionette.switch_to_window(handle)
                self.marionette.close_chrome_window()

        self.marionette.switch_to_window(start_tab)

        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(len(self.marionette.window_handles), 1)
        self.assertEqual(self.marionette.current_window_handle, start_tab)

    def test_tab_and_window_handles(self):
        start_tab = self.marionette.current_window_handle
        start_chrome_window = self.marionette.current_chrome_window_handle
        tab_open_page = self.marionette.absolute_url("windowHandles.html")
        window_open_page = self.marionette.absolute_url("test_windows.html")

        # Open a new tab and switch to it.
        self.marionette.navigate(tab_open_page)
        link = self.marionette.find_element(By.ID, "new-tab")
        link.click()

        self.wait_for_condition(lambda mn: len(mn.window_handles) == 2)
        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, start_chrome_window)

        handles = self.marionette.window_handles
        handles.remove(start_tab)

        new_tab = handles.pop()
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:blank")

        # Open a new window from the new tab.
        self.marionette.navigate(window_open_page)

        link = self.marionette.find_element(By.LINK_TEXT, "Open new window")
        link.click()
        self.wait_for_condition(lambda mn: len(mn.window_handles) == 3)

        self.assertEqual(len(self.marionette.chrome_window_handles), 2)
        self.assertEqual(self.marionette.current_chrome_window_handle, start_chrome_window)

        # Find the new window and switch to it.
        handles = self.marionette.window_handles
        handles.remove(start_tab)
        handles.remove(new_tab)
        new_window = handles.pop()

        self.marionette.switch_to_window(new_window)
        results_page = self.marionette.absolute_url("resultPage.html")
        self.assertEqual(self.marionette.get_url(), results_page)

        self.assertEqual(len(self.marionette.chrome_window_handles), 2)
        self.assertNotEqual(self.marionette.current_chrome_window_handle, start_chrome_window)

        # Return to our original tab and close it.
        self.marionette.switch_to_window(start_tab)
        self.marionette.close()
        self.assertEquals(len(self.marionette.window_handles), 2)

        # Close the opened window and carry on in our new tab.
        self.marionette.switch_to_window(new_window)
        self.marionette.close()
        self.assertEqual(len(self.marionette.window_handles), 1)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), results_page)
        self.marionette.navigate("about:blank")

        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, start_chrome_window)
