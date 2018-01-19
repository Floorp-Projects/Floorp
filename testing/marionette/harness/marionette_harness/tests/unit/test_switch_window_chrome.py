# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys
from unittest import skipIf

from marionette_driver import By

# add this directory to the path
sys.path.append(os.path.dirname(__file__))

from test_switch_window_content import TestSwitchToWindowContent


class TestSwitchWindowChrome(TestSwitchToWindowContent):

    def setUp(self):
        super(TestSwitchWindowChrome, self).setUp()

        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()

        super(TestSwitchWindowChrome, self).tearDown()

    def open_window_in_background(self):
        with self.marionette.using_context("chrome"):
            self.marionette.execute_script("""
              window.open("about:blank", null, "location=1,toolbar=1");
              window.focus();
            """)

    def open_window_in_foreground(self):
        with self.marionette.using_context("content"):
            self.marionette.navigate(self.test_page)
            link = self.marionette.find_element(By.ID, "new-window")
            link.click()

    @skipIf(sys.platform.startswith("linux"),
            "Bug 1335457 - Fails to open a background window on Linux")
    def test_switch_tabs_for_new_background_window_without_focus_change(self):
        # Bug 1334981 - with testmode enabled getMostRecentWindow detects the wrong window
        with self.marionette.using_prefs({"focusmanager.testmode": False}):
            # Open an addition tab in the original window so we can better check
            # the selected index in thew new window to be opened.
            second_tab = self.open_tab(trigger=self.open_tab_in_foreground)
            self.marionette.switch_to_window(second_tab, focus=True)
            second_tab_index = self.get_selected_tab_index()
            self.assertNotEqual(second_tab_index, self.selected_tab_index)

            # Opens a new background window, but we are interested in the tab
            tab_in_new_window = self.open_tab(trigger=self.open_window_in_background)
            self.assertEqual(self.marionette.current_window_handle, second_tab)
            self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
            self.assertEqual(self.get_selected_tab_index(), second_tab_index)
            with self.marionette.using_context("content"):
                self.assertEqual(self.marionette.get_url(), self.empty_page)

            # Switch to the tab in the new window but don't focus it
            self.marionette.switch_to_window(tab_in_new_window, focus=False)
            self.assertEqual(self.marionette.current_window_handle, tab_in_new_window)
            self.assertNotEqual(self.marionette.current_chrome_window_handle, self.start_window)
            self.assertEqual(self.get_selected_tab_index(), second_tab_index)
            with self.marionette.using_context("content"):
                self.assertEqual(self.marionette.get_url(), "about:blank")

    def test_switch_tabs_for_new_foreground_window_with_focus_change(self):
        # Open an addition tab in the original window so we can better check
        # the selected index in thew new window to be opened.
        second_tab = self.open_tab(trigger=self.open_tab_in_foreground)
        self.marionette.switch_to_window(second_tab, focus=True)
        second_tab_index = self.get_selected_tab_index()
        self.assertNotEqual(second_tab_index, self.selected_tab_index)

        # Opens a new window, but we are interested in the tab
        tab_in_new_window = self.open_tab(trigger=self.open_window_in_foreground)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        self.marionette.switch_to_window(tab_in_new_window)
        self.assertEqual(self.marionette.current_window_handle, tab_in_new_window)
        self.assertNotEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.empty_page)

        self.marionette.switch_to_window(second_tab, focus=True)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        # Bug 1335085 - The focus doesn't change even as requested so.
        # self.assertEqual(self.get_selected_tab_index(), second_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

    def test_switch_tabs_for_new_foreground_window_without_focus_change(self):
        # Open an addition tab in the original window so we can better check
        # the selected index in thew new window to be opened.
        second_tab = self.open_tab(trigger=self.open_tab_in_foreground)
        self.marionette.switch_to_window(second_tab, focus=True)
        second_tab_index = self.get_selected_tab_index()
        self.assertNotEqual(second_tab_index, self.selected_tab_index)

        # Opens a new window, but we are interested in the tab which automatically
        # gets the focus.
        self.open_tab(trigger=self.open_window_in_foreground)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)

        # Switch to the second tab in the first window, but don't focus it.
        self.marionette.switch_to_window(second_tab, focus=False)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(self.marionette.current_chrome_window_handle, self.start_window)
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)
        with self.marionette.using_context("content"):
            self.assertEqual(self.marionette.get_url(), self.test_page)
