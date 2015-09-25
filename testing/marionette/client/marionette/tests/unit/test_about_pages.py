# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette.marionette_test import skip_if_e10s
from marionette_driver.keys import Keys
from marionette_driver.by import By

class TestAboutPages(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)

        if self.marionette.session_capabilities['platformName'] == 'DARWIN':
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.remote_uri = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(self.remote_uri)

    def test_back_forward(self):
        self.marionette.navigate("about:blank")
        self.marionette.navigate(self.remote_uri)

        self.marionette.navigate("about:preferences")

        self.marionette.go_back()

        self.wait_for_condition(
            lambda mn: mn.get_url() == self.remote_uri)

    def test_navigate_non_remote_about_pages(self):
        self.marionette.navigate("about:blank")
        self.assertEqual(self.marionette.get_url(), "about:blank")
        self.marionette.navigate("about:preferences")
        self.assertEqual(self.marionette.get_url(), "about:preferences")

    def test_navigate_shortcut_key(self):
        self.marionette.navigate("about:preferences")
        self.marionette.navigate(self.remote_uri)
        self.marionette.navigate("about:blank")

        start_win = self.marionette.current_window_handle
        start_win_handles = self.marionette.window_handles
        with self.marionette.using_context("chrome"):
            main_win = self.marionette.find_element("id", "main-window")
            main_win.send_keys(self.mod_key, Keys.SHIFT, 'a')

        self.wait_for_condition(lambda mn: len(mn.window_handles) == 2)
        self.assertEqual(start_win, self.marionette.current_window_handle)
        [new_tab] = list(set(self.marionette.window_handles) - set(start_win_handles))
        self.marionette.switch_to_window(new_tab)
        self.wait_for_condition(lambda mn: mn.get_url() == "about:addons")
        self.marionette.close()
        self.marionette.switch_to_window(start_win)

    @skip_if_e10s
    def test_type_to_non_remote_tab(self):
        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element('id', 'urlbar')
            urlbar.send_keys(self.mod_key + 'a')
            urlbar.send_keys(self.mod_key + 'x')
            urlbar.send_keys('about:preferences' + Keys.ENTER)
        self.wait_for_condition(lambda mn: mn.get_url() == "about:preferences")

    def test_type_to_remote_tab(self):
        self.marionette.navigate("about:preferences")
        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element('id', 'urlbar')
            urlbar.send_keys(self.mod_key + 'a')
            urlbar.send_keys(self.mod_key + 'x')
            urlbar.send_keys(self.remote_uri + Keys.ENTER)

        self.wait_for_condition(lambda mn: mn.get_url() == self.remote_uri)

    def test_hang(self):
        self.marionette.set_context('chrome')
        initial_tab = self.marionette.window_handles[0]

        # Open a new tab and close the first one
        new_tab = self.marionette.find_element(By.ID, 'menu_newNavigatorTab')
        new_tab.click()

        new_tab = [handle for handle in self.marionette.window_handles if
                   handle != initial_tab][0]

        self.marionette.close()
        self.marionette.switch_to_window(new_tab)

        with self.marionette.using_context('content'):
            self.marionette.navigate(self.remote_uri)
