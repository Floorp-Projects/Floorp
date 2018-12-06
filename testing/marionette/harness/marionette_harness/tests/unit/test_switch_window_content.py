# This Source Code Form is subject to the terms of the Mozilla ublic
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSwitchToWindowContent(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSwitchToWindowContent, self).setUp()

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.selected_tab_index = self.get_selected_tab_index()

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
                  Components.utils.import("resource:///modules/BrowserWindowTracker.jsm");
                  win = BrowserWindowTracker.getTopWindow();
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

    def test_switch_tabs_with_focus_change(self):
        new_tab = self.open_tab(focus=True)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        # Switch to new tab first because it is already selected
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        # Switch to original tab by explicitely setting the focus
        self.marionette.switch_to_window(self.start_tab, focus=True)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)

    def test_switch_tabs_without_focus_change(self):
        new_tab = self.open_tab(focus=True)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        # Switch to new tab first because it is already selected
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)

        # Switch to original tab by explicitely not setting the focus
        self.marionette.switch_to_window(self.start_tab, focus=False)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)

        self.marionette.switch_to_window(new_tab)
        self.marionette.close()

        self.marionette.switch_to_window(self.start_tab)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)

    def test_switch_from_content_to_chrome_window_should_not_change_selected_tab(self):
        new_tab = self.open_tab(focus=True)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        new_tab_index = self.get_selected_tab_index()

        self.marionette.switch_to_window(self.start_window)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        self.assertEqual(self.get_selected_tab_index(), new_tab_index)

    def test_switch_to_new_private_browsing_tab(self):
        # Test that tabs (browsers) are correctly registered for a newly opened
        # private browsing window/tab. This has to also happen without explicitely
        # switching to the tab itself before using any commands in content scope.
        #
        # Note: Not sure why this only affects private browsing windows only.
        new_tab = self.open_tab(focus=True)
        self.marionette.switch_to_window(new_tab)

        def open_private_browsing_window_firefox():
            with self.marionette.using_context("content"):
                self.marionette.find_element(By.ID, "startPrivateBrowsing").click()

        def open_private_browsing_tab_fennec():
            with self.marionette.using_context("content"):
                self.marionette.find_element(By.ID, "newPrivateTabLink").click()

        with self.marionette.using_context("content"):
            self.marionette.navigate("about:privatebrowsing")
            if self.marionette.session_capabilities["browserName"] == "fennec":
                new_pb_tab = self.open_tab(open_private_browsing_tab_fennec)
            else:
                new_pb_tab = self.open_tab(open_private_browsing_window_firefox)

        self.marionette.switch_to_window(new_pb_tab)
        self.assertEqual(self.marionette.current_window_handle, new_pb_tab)

        self.marionette.execute_script(" return true; ")
