# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys

from unittest import skipIf

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

    def test_switch_to_unloaded_tab(self):
        # Can only run in content context
        pass

    @skipIf(
        sys.platform.startswith("linux"),
        "Bug 1511970 - New window isn't moved to the background on Linux",
    )
    def test_switch_tabs_for_new_background_window_without_focus_change(self):
        # Open an additional tab in the original window so we can better check
        # the selected index in thew new window to be opened.
        second_tab = self.open_tab(focus=True)
        self.marionette.switch_to_window(second_tab, focus=True)
        second_tab_index = self.get_selected_tab_index()
        self.assertNotEqual(second_tab_index, self.selected_tab_index)

        # Open a new background window, but we are interested in the tab
        with self.marionette.using_context("content"):
            tab_in_new_window = self.open_window()
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertEqual(self.get_selected_tab_index(), second_tab_index)

        # Switch to the tab in the new window but don't focus it
        self.marionette.switch_to_window(tab_in_new_window, focus=False)
        self.assertEqual(self.marionette.current_window_handle, tab_in_new_window)
        self.assertNotEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertEqual(self.get_selected_tab_index(), second_tab_index)

    def test_switch_tabs_for_new_foreground_window_with_focus_change(self):
        # Open an addition tab in the original window so we can better check
        # the selected index in thew new window to be opened.
        second_tab = self.open_tab()
        self.marionette.switch_to_window(second_tab, focus=True)
        second_tab_index = self.get_selected_tab_index()
        self.assertNotEqual(second_tab_index, self.selected_tab_index)

        # Opens a new window, but we are interested in the tab
        with self.marionette.using_context("content"):
            tab_in_new_window = self.open_window(focus=True)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)

        self.marionette.switch_to_window(tab_in_new_window)
        self.assertEqual(self.marionette.current_window_handle, tab_in_new_window)
        self.assertNotEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)

        self.marionette.switch_to_window(second_tab, focus=True)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        # Bug 1335085 - The focus doesn't change even as requested so.
        # self.assertEqual(self.get_selected_tab_index(), second_tab_index)

    def test_switch_tabs_for_new_foreground_window_without_focus_change(self):
        # Open an addition tab in the original window so we can better check
        # the selected index in thew new window to be opened.
        second_tab = self.open_tab()
        self.marionette.switch_to_window(second_tab, focus=True)
        second_tab_index = self.get_selected_tab_index()
        self.assertNotEqual(second_tab_index, self.selected_tab_index)

        self.open_window(focus=True)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)

        # Switch to the second tab in the first window, but don't focus it.
        self.marionette.switch_to_window(second_tab, focus=False)
        self.assertEqual(self.marionette.current_window_handle, second_tab)
        self.assertEqual(
            self.marionette.current_chrome_window_handle, self.start_window
        )
        self.assertNotEqual(self.get_selected_tab_index(), second_tab_index)
