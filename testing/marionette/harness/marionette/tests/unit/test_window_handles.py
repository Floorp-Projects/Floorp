# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase, WindowManagerMixin
from marionette_driver.keys import Keys
from marionette_driver import By, Wait


class TestWindowHandles(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestWindowHandles, self).setUp()

        self.test_page = self.marionette.absolute_url("windowHandles.html")
        self.marionette.navigate(self.test_page)

    def tearDown(self):
        self.close_all_windows()
        self.close_all_tabs()

        super(TestWindowHandles, self).tearDown()

    def test_new_tab_window_handles(self):
        keys = []
        if self.marionette.session_capabilities['platformName'] == 'darwin':
            keys.append(Keys.META)
        else:
            keys.append(Keys.CONTROL)
        keys.append('t')

        def open_with_shortcut():
            with self.marionette.using_context("chrome"):
                main_win = self.marionette.find_element(By.ID, "main-window")
                main_win.send_keys(*keys)

        new_tab = self.open_tab(trigger=open_with_shortcut)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:newtab")

        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    def test_new_tab_window_handles_no_switch(self):
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
            with self.marionette.using_context("chrome"):
                menu_new_tab = self.marionette.find_element(By.ID, 'menu_newNavigatorTab')
                menu_new_tab.click()

        new_tab = self.open_tab(trigger=open_with_menu)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        # We still have the default tab set as our window handle. This
        # get_url command should be sent immediately, and not be forever-queued.
        self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    def test_link_opened_tab_window_handles(self):
        def open_with_link():
            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

        new_tab = self.open_tab(trigger=open_with_link)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:blank")
        self.assertEqual(self.marionette.title, "")

        self.marionette.switch_to_window(self.start_tab)

        self.assertEqual(self.marionette.get_url(), self.test_page)
        self.assertEqual(self.marionette.title, "Marionette New Tab Link")

        self.marionette.close()
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:blank")

    def test_chrome_windows(self):
        # We open a chrome window but are actually interested in the new tab.
        new_window = self.open_window(
            trigger=lambda: self.marionette.find_element(By.ID, "new-window").click())
        with self.marionette.using_context("chrome"):
            self.marionette.switch_to_window(new_window)

        # Check that the new tab is available and wait until it has been loaded.
        self.assertEqual(len(self.marionette.window_handles), len(self.start_tabs) + 1)
        new_tab = self.marionette.current_window_handle
        Wait(self.marionette).until(lambda _: self.marionette.get_url() == self.test_page,
                                    message="Test page hasn't been loaded for newly opened tab")

        link_new_tab = self.marionette.find_element(By.ID, "new-tab")
        for i in range(3):
            self.open_tab(trigger=lambda: link_new_tab.click())
            self.marionette.switch_to_window(new_tab)
            # No more chrome windows should be opened
            self.assertEqual(len(self.marionette.chrome_window_handles),
                             len(self.start_windows) + 1)

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_chrome_window_handles_with_scopes(self):
        # Ensure that we work in chrome scope so we don't have any limitations
        with self.marionette.using_context("chrome"):
            # Open a browser and a non-browser (about window) chrome window
            self.open_window(
                trigger=lambda: self.marionette.execute_script("window.open();"))
            self.open_window(
                trigger=lambda: self.marionette.find_element(By.ID, "aboutName").click())

            handles_in_chrome_scope = self.marionette.chrome_window_handles
            with self.marionette.using_context("content"):
                self.assertEqual(self.marionette.chrome_window_handles,
                                 handles_in_chrome_scope)

    def test_tab_and_window_handles(self):
        window_open_page = self.marionette.absolute_url("test_windows.html")
        results_page = self.marionette.absolute_url("resultPage.html")

        # Open a new tab and switch to it.
        def open_tab_with_link():
            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

        second_tab = self.open_tab(trigger=open_tab_with_link)
        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        self.marionette.switch_to_window(second_tab)
        self.assertEqual(self.marionette.get_url(), "about:blank")

        # Open a new window from the new tab and only care about the second new tab
        def open_window_with_link():
            link = self.marionette.find_element(By.LINK_TEXT, "Open new window")
            link.click()

        # We open a new window but are actually interested in the new tab
        self.marionette.navigate(window_open_page)
        third_tab = self.open_tab(trigger=open_window_with_link)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)

        # Check that the new tab has the correct page loaded
        self.marionette.switch_to_window(third_tab)
        self.assertEqual(self.marionette.get_url(), results_page)

        self.assertNotEqual(self.marionette.current_chrome_window_handle, self.start_window)

        # Return to our original tab and close it.
        self.marionette.switch_to_window(self.start_tab)
        self.marionette.close()
        self.assertEquals(len(self.marionette.window_handles), 2)

        # Close the opened window and carry on in our second tab.
        self.marionette.switch_to_window(third_tab)
        self.marionette.close()
        self.assertEqual(len(self.marionette.window_handles), 1)

        self.marionette.switch_to_window(second_tab)
        self.assertEqual(self.marionette.get_url(), results_page)
        self.marionette.navigate("about:blank")

        self.assertEqual(len(self.marionette.chrome_window_handles), 1)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
