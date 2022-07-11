# This Source Code Form is subject to the terms of the Mozilla ublic
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import sys
from unittest import skipIf

from six.moves.urllib.parse import quote

from marionette_driver import By, Wait
from marionette_driver.keys import Keys

from marionette_harness import (
    MarionetteTestCase,
    WindowManagerMixin,
)


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


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
            return self.marionette.execute_script(
                """
                const { AppConstants } = ChromeUtils.import(
                  "resource://gre/modules/AppConstants.jsm"
                );

                let win = null;

                if (AppConstants.MOZ_APP_NAME == "fennec") {
                  win = Services.wm.getMostRecentWindow("navigator:browser");
                } else {
                  const { BrowserWindowTracker } = ChromeUtils.import(
                    "resource:///modules/BrowserWindowTracker.jsm"
                  );
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
            """
            )

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

    @skipIf(
        sys.platform.startswith("linux"),
        "Bug 1557232 - Original window sometimes doesn't receive focus",
    )
    def test_switch_tabs_in_different_windows_with_focus_change(self):
        new_tab1 = self.open_tab(focus=True)
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), 1)

        # Switch to new tab first which is already selected
        self.marionette.switch_to_window(new_tab1)
        self.assertEqual(self.marionette.current_window_handle, new_tab1)
        self.assertEqual(self.get_selected_tab_index(), 1)

        # Open a new browser window with a single focused tab already focused
        with self.marionette.using_context("content"):
            new_tab2 = self.open_window(focus=True)
        self.assertEqual(self.marionette.current_window_handle, new_tab1)
        self.assertEqual(self.get_selected_tab_index(), 0)

        # Switch to that tab
        self.marionette.switch_to_window(new_tab2)
        self.assertEqual(self.marionette.current_window_handle, new_tab2)
        self.assertEqual(self.get_selected_tab_index(), 0)

        # Switch back to the 2nd tab of the original window and setting the focus
        self.marionette.switch_to_window(new_tab1, focus=True)
        self.assertEqual(self.marionette.current_window_handle, new_tab1)
        self.assertEqual(self.get_selected_tab_index(), 1)

        self.marionette.switch_to_window(new_tab2)
        self.marionette.close()

        self.marionette.switch_to_window(new_tab1)
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

    def test_switch_to_unloaded_tab(self):
        first_page = inline("<p>foo")
        second_page = inline("<p>bar")

        self.assertEqual(len(self.marionette.window_handles), 1)
        self.marionette.navigate(first_page)

        new_tab = self.open_tab()
        self.assertEqual(self.marionette.current_window_handle, self.start_tab)
        self.assertEqual(self.get_selected_tab_index(), self.selected_tab_index)

        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.current_window_handle, new_tab)
        self.assertNotEqual(self.get_selected_tab_index(), self.selected_tab_index)
        self.marionette.navigate(second_page)

        # The restart will cause the background tab to stay unloaded
        self.marionette.restart(in_app=True)
        self.assertEqual(len(self.marionette.window_handles), 2)

        # Refresh window handles
        window_handles = self.marionette.window_handles
        self.assertEqual(len(window_handles), 2)

        current_tab = self.marionette.current_window_handle
        [other_tab] = filter(lambda handle: handle != current_tab, window_handles)

        Wait(self.marionette, timeout=5).until(
            lambda _: self.marionette.get_url() == second_page,
            message="Expected URL in the second tab has been loaded",
        )

        self.marionette.switch_to_window(other_tab)
        Wait(self.marionette, timeout=5).until(
            lambda _: self.marionette.get_url() == first_page,
            message="Expected URL in the first tab has been loaded",
        )

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

    def test_switch_to_window_after_remoteness_change(self):
        # Test that after a remoteness change (and a browsing context swap)
        # marionette can still switch to tabs correctly.
        with self.marionette.using_context("content"):
            # about:robots runs in a different process and will trigger a
            # remoteness change with or without fission.
            self.marionette.navigate("about:robots")

        about_robots_tab = self.marionette.current_window_handle

        # Open a new tab and switch to it before trying to switch back to the
        # initial tab.
        tab2 = self.open_tab(focus=True)
        self.marionette.switch_to_window(tab2)
        self.marionette.close()

        self.marionette.switch_to_window(about_robots_tab)
        self.assertEqual(self.marionette.current_window_handle, about_robots_tab)
