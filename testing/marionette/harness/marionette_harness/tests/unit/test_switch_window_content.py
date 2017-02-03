# This Source Code Form is subject to the terms of the Mozilla ublic
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Actions, By
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSwitchToWindowContent(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSwitchToWindowContent, self).setUp()

        if self.marionette.session_capabilities["platformName"] == "darwin":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.empty_page = self.marionette.absolute_url("empty.html")
        self.test_page = self.marionette.absolute_url("windowHandles.html")

        self.selected_tab_index = self.get_selected_tab_index()

        with self.marionette.using_context("content"):
            self.marionette.navigate(self.test_page)

    def tearDown(self):
        self.close_all_tabs()

        super(TestSwitchToWindowContent, self).tearDown()

    def get_selected_tab_index(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script("""
                Components.utils.import("resource://gre/modules/AppConstants.jsm");

                let win = null;

                if (AppConstants.MOZ_APP_NAME == "fennec") {
                  Components.utils.import("resource://gre/modules/Services.jsm");
                  win = Services.wm.getMostRecentWindow("navigator:browser");
                } else {
                  Components.utils.import("resource:///modules/RecentWindow.jsm");
                  win = RecentWindow.getMostRecentBrowserWindow();
                }

                let tabBrowser = null;

                // Fennec
                if (win.BrowserApp) {
                  tabBrowser = win.BrowserApp;

                // Firefox
                } else if (win.gBrowser) {
                  tabBrowser = win.gBrowser;

                } else {
                  return null;
                }

                for (let i = 0; i < tabBrowser.tabs.length; i++) {
                  if (tabBrowser.tabs[i] == tabBrowser.selectedTab) {
                    return i;
                  }
                }
            """)

    def open_tab_in_background(self):
        with self.marionette.using_context("content"):
            link = self.marionette.find_element(By.ID, "new-tab")

            action = Actions(self.marionette)
            action.key_down(self.mod_key).click(link).perform()

    def open_tab_in_foreground(self):
        with self.marionette.using_context("content"):
            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

    def test_switch_tabs_with_focus_change(self):
        new_tab = self.open_tab(self.open_tab_in_foreground)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

        self.marionette.switch_to_window(self.start_tab, focus=True)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

    def test_switch_tabs_without_focus_change(self):
        new_tab = self.open_tab(self.open_tab_in_foreground)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        # Switch to new tab first because it is already selected
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        self.marionette.switch_to_window(self.start_tab, focus=False)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

    def test_switch_from_content_to_chrome_window_should_not_change_selected_tab(self):
        new_tab = self.open_tab(self.open_tab_in_foreground)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        new_tab_index = self.get_selected_tab_index()

        self.marionette.switch_to_window(self.start_window)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        self.assertEqual(self.get_selected_tab_index(), new_tab_index)
