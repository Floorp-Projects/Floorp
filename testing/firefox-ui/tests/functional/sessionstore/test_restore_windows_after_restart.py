# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_ui_harness.testcases import FirefoxTestCase


class TestRestoreWindowsAfterRestart(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Each list element represents a window of tabs loaded at
        # some testing URL
        self.test_windows = set([
            # Window 1. Note the comma after the absolute_url call -
            # this is Python's way of declaring a 1 item tuple.
            (self.marionette.absolute_url('layout/mozilla.html'), ),

            # Window 2
            (self.marionette.absolute_url('layout/mozilla_organizations.html'),
             self.marionette.absolute_url('layout/mozilla_community.html')),

            # Window 3
            (self.marionette.absolute_url('layout/mozilla_governance.html'),
             self.marionette.absolute_url('layout/mozilla_grants.html')),
        ])

        self.private_windows = set([
            (self.marionette.absolute_url('layout/mozilla_mission.html'),
             self.marionette.absolute_url('layout/mozilla_organizations.html')),

            (self.marionette.absolute_url('layout/mozilla_projects.html'),
             self.marionette.absolute_url('layout/mozilla_mission.html')),
        ])

        self.marionette.enforce_gecko_prefs({
            # Set browser to restore previous session
            'browser.startup.page': 3,
            # Make the content load right away instead of waiting for
            # the user to click on the background tabs
            'browser.sessionstore.restore_on_demand': False,
            # Avoid race conditions by having the content process never
            # send us session updates unless the parent has explicitly asked
            # for them via the TabStateFlusher.
            'browser.sessionstore.debug.no_auto_updates': True,
        })

    def tearDown(self):
        try:
            # Create a fresh profile for subsequent tests.
            self.restart(clean=True)
        finally:
            FirefoxTestCase.tearDown(self)

    def test_with_variety(self):
        """ Opens a set of windows, both standard and private, with
        some number of tabs in them. Once the tabs have loaded, restarts
        the browser, and then ensures that the standard tabs have been
        restored, and that the private ones have not.
        """
        self.open_windows(self.test_windows)
        self.open_windows(self.private_windows, is_private=True)

        self.restart()

        windows = self.windows.all

        # There's no guarantee that Marionette will return us an
        # iterator for the opened windows that will match the
        # order within our window list. Instead, we'll convert
        # the list of URLs within each open window to a set of
        # tuples that will allow us to do a direct comparison
        # while allowing the windows to be in any order.
        opened_windows = set()
        for win in windows:
            urls = tuple()
            for tab in win.tabbar.tabs:
                urls = urls + tuple([tab.location])
            opened_windows.add(urls)

        self.assertEqual(opened_windows, self.test_windows)

    def open_windows(self, window_sets, is_private=False):
        """ Opens a set of windows with tabs pointing at some
        URLs.

        @param window_sets (list)
               A set of URL tuples. Each tuple within window_sets
               represents a window, and each URL in the URL
               tuples represents what will be loaded in a tab.

               Note that if is_private is False, then the first
               URL tuple will be opened in the current window, and
               subequent tuples will be opened in new windows.

               Example:

               set(
                   (self.marionette.absolute_url('layout/mozilla_1.html'),
                    self.marionette.absolute_url('layout/mozilla_2.html')),

                   (self.marionette.absolute_url('layout/mozilla_3.html'),
                    self.marionette.absolute_url('layout/mozilla_4.html')),
               )

               This would take the currently open window, and load
               mozilla_1.html and mozilla_2.html in new tabs. It would
               then open a new, second window, and load tabs at
               mozilla_3.html and mozilla_4.html.
        @param is_private (boolean, optional)
               Whether or not any new windows should be a private browsing
               windows.
        """

        if (is_private):
            win = self.browser.open_browser(is_private=True)
            win.switch_to()
        else:
            win = self.browser

        for index, urls in enumerate(window_sets):
            if index > 0:
                win = self.browser.open_browser(is_private=is_private)
            win.switch_to()
            self.open_tabs(win, urls)

    def open_tabs(self, win, urls):
        """ Opens a set of URLs inside a window in new tabs.

        @param win (browser window)
               The browser window to load the tabs in.
        @param urls (tuple)
               A tuple of URLs to load in this window. The
               first URL will be loaded in the currently selected
               browser tab. Subsequent URLs will be loaded in
               new tabs.
        """
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
