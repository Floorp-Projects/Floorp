# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, skip_if_mobile, WindowManagerMixin


class TestAboutPages(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAboutPages, self).setUp()

        if self.marionette.session_capabilities['platformName'] == 'darwin':
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.remote_uri = self.marionette.absolute_url("windowHandles.html")

    def tearDown(self):
        self.close_all_tabs()

        super(TestAboutPages, self).tearDown()

    def open_tab_with_link(self):
        with self.marionette.using_context("content"):
            self.marionette.navigate(self.remote_uri)

            link = self.marionette.find_element(By.ID, "new-tab")
            link.click()

    @skip_if_mobile("Bug 1333209 - Process killed because of connection loss")
    def test_back_forward(self):
        # Bug 1311041 - Prevent changing of window handle by forcing the test
        # to be run in a new tab.
        new_tab = self.open_tab(trigger=self.open_tab_with_link)
        self.marionette.switch_to_window(new_tab)

        self.marionette.navigate("about:blank")
        self.marionette.navigate(self.remote_uri)
        self.marionette.navigate("about:support")

        self.marionette.go_back()
        Wait(self.marionette).until(lambda mn: mn.get_url() == self.remote_uri,
                                    message="'{}' hasn't been loaded".format(self.remote_uri))

        # Bug 1332998 - Timeout loading the page
        # self.marionette.go_forward()
        # Wait(self.marionette).until(lambda mn: mn.get_url() == self.remote_uri,
        #                             message="'about:support' hasn't been loaded")

        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    @skip_if_mobile("Bug 1333209 - Process killed because of connection loss")
    def test_navigate_non_remote_about_pages(self):
        # Bug 1311041 - Prevent changing of window handle by forcing the test
        # to be run in a new tab.
        new_tab = self.open_tab(trigger=self.open_tab_with_link)
        self.marionette.switch_to_window(new_tab)

        self.marionette.navigate("about:blank")
        self.assertEqual(self.marionette.get_url(), "about:blank")
        self.marionette.navigate("about:support")
        self.assertEqual(self.marionette.get_url(), "about:support")

        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    @skip_if_mobile("On Android no shortcuts are available")
    def test_navigate_shortcut_key(self):
        def open_with_shortcut():
            self.marionette.navigate(self.remote_uri)
            with self.marionette.using_context("chrome"):
                main_win = self.marionette.find_element(By.ID, "main-window")
                main_win.send_keys(self.mod_key, Keys.SHIFT, 'a')

        new_tab = self.open_tab(trigger=open_with_shortcut)
        self.marionette.switch_to_window(new_tab)

        Wait(self.marionette).until(lambda mn: mn.get_url() == "about:addons",
                                    message="'about:addons' hasn't been loaded")

        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    @skip_if_mobile("Interacting with chrome elements not available for Fennec")
    def test_type_to_non_remote_tab(self):
        # Bug 1311041 - Prevent changing of window handle by forcing the test
        # to be run in a new tab.
        new_tab = self.open_tab(trigger=self.open_tab_with_link)
        self.marionette.switch_to_window(new_tab)

        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, 'urlbar')
            urlbar.send_keys(self.mod_key + 'a')
            urlbar.send_keys(self.mod_key + 'x')
            urlbar.send_keys('about:support' + Keys.ENTER)
        Wait(self.marionette).until(lambda mn: mn.get_url() == "about:support",
                                    message="'about:support' hasn't been loaded")

        self.marionette.close()
        self.marionette.switch_to_window(self.start_tab)

    @skip_if_mobile("Interacting with chrome elements not available for Fennec")
    def test_type_to_remote_tab(self):
        # Bug 1311041 - Prevent changing of window handle by forcing the test
        # to be run in a new tab.
        new_tab = self.open_tab(trigger=self.open_tab_with_link)
        self.marionette.switch_to_window(new_tab)

        # about:blank keeps remoteness from remote_uri
        self.marionette.navigate("about:blank")
        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, 'urlbar')
            urlbar.send_keys(self.mod_key + 'a')
            urlbar.send_keys(self.mod_key + 'x')
            urlbar.send_keys(self.remote_uri + Keys.ENTER)

        Wait(self.marionette).until(lambda mn: mn.get_url() == self.remote_uri,
                                    message="'{}' hasn't been loaded".format(self.remote_uri))

    @skip_if_mobile("Needs application independent method to open a new tab")
    def test_hang(self):
        # Bug 1311041 - Prevent changing of window handle by forcing the test
        # to be run in a new tab.
        new_tab = self.open_tab(trigger=self.open_tab_with_link)

        # Close the start tab
        self.marionette.close()
        self.marionette.switch_to_window(new_tab)

        self.marionette.navigate(self.remote_uri)
