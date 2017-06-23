# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from firefox_puppeteer import PuppeteerMixin
from marionette_harness import MarionetteTestCase
from marionette_driver import Wait


class TestBaseTabbarSessionRestoreButton(PuppeteerMixin, MarionetteTestCase):
    def setUp(self, prefValue=True):
        super(TestBaseTabbarSessionRestoreButton, self).setUp()
        self.marionette.enforce_gecko_prefs({'browser.tabs.restorebutton': prefValue})

        # Each list element represents a window of tabs loaded at
        # some testing URL, the URLS are arbitrary.
        self.test_windows = set([
            (self.marionette.absolute_url('layout/mozilla_projects.html'),
             self.marionette.absolute_url('layout/mozilla.html'),
             self.marionette.absolute_url('layout/mozilla_mission.html')),
        ])

        self.open_windows(self.test_windows)

        self.marionette.quit(in_app=True)
        self.marionette.start_session()
        self.marionette.set_context('chrome')

    def open_windows(self, window_sets):
        win = self.browser
        for index, urls in enumerate(window_sets):
            if index > 0:
                win = self.browser.open_browser()
            win.switch_to()
            self.open_tabs(win, urls)

    def open_tabs(self, win, urls):
        # If there are any remaining URLs for this window,
        # open some new tabs and navigate to them.
        with self.marionette.using_context('content'):
            if isinstance(urls, str):
                self.marionette.navigate(urls)
            else:
                for index, url in enumerate(urls):
                    if index > 0:
                        with self.marionette.using_context('chrome'):
                            win.tabbar.open_tab()
                    self.marionette.navigate(url)

    def tearDown(self):
        try:
            # Create a fresh profile for subsequent tests.
            self.restart(clean=True)
        finally:
            super(TestBaseTabbarSessionRestoreButton, self).tearDown()


class TestTabbarSessionRestoreButton(TestBaseTabbarSessionRestoreButton):
    def setUp(self):
        super(TestTabbarSessionRestoreButton, self).setUp()

    def test_session_exists(self):
        wrapper = self.puppeteer.windows.current.tabbar.restore_tabs_button_wrapper

        # A session-exists attribute has been added to the button,
        # which allows it to show.
        self.assertEqual(wrapper.get_attribute('session-exists'), 'true')

    def test_window_resizing(self):
        wrapper = self.puppeteer.windows.current.tabbar.restore_tabs_button_wrapper

        # Ensure the window is large enough to show the button.
        self.marionette.set_window_size(1200, 1200)
        self.assertEqual(wrapper.value_of_css_property('visibility'), 'visible')

        # Set the window small enough that the button disappears.
        self.marionette.set_window_size(335, 335)
        self.assertEqual(wrapper.value_of_css_property('visibility'), 'hidden')

    def test_click_restore(self):
        button = self.puppeteer.windows.current.tabbar.restore_tabs_button

        # The new browser window is not the same window as last time,
        # and did not automatically restore the session, so there is only one tab.
        self.assertEqual(len(self.puppeteer.windows.current.tabbar.tabs), 1)

        button.click()

        Wait(self.marionette, timeout=20).until(
            lambda _: len(self.puppeteer.windows.current.tabbar.tabs) > 1,
            message='there is only one tab, so the session was not restored')

        # After clicking the button to restore the session,
        # there is more than one tab.
        self.assertTrue(len(self.puppeteer.windows.current.tabbar.tabs) > 1)


class TestNoTabbarSessionRestoreButton(TestBaseTabbarSessionRestoreButton):
    def setUp(self):
        super(TestNoTabbarSessionRestoreButton, self).setUp(False)

    def test_pref_off_button_does_not_show(self):
        wrapper = self.puppeteer.windows.current.tabbar.restore_tabs_button_wrapper

        # A session-exists attribute is not on the button,
        # since the button will never show itself with the pref off.
        self.assertEqual(wrapper.get_attribute('session-exists'), '')
