# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.ui.browser.window import BrowserWindow
from marionette_driver import By, Wait
from marionette_harness import MarionetteTestCase


class TestAboutPrivateBrowsing(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAboutPrivateBrowsing, self).setUp()

        # Use a fake local support URL
        support_url = 'about:blank?'
        self.marionette.set_pref('app.support.baseURL', support_url)
        self.pb_url = support_url + 'private-browsing'

        # Disable the search UI
        self.marionette.set_pref('browser.privatebrowsing.searchUI', False)

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
            self.browser.switch_to()

            self.marionette.clear_pref('app.support.baseURL')
            self.marionette.clear_pref('browser.privatebrowsing.searchUI')
        finally:
            super(TestAboutPrivateBrowsing, self).tearDown()

    def testCheckAboutPrivateBrowsing(self):
        self.assertFalse(self.browser.is_private)

        with self.marionette.using_context('content'):
            self.marionette.navigate('about:privatebrowsing')

            # Disabled awaiting support for Fluent strings in firefox-ui tests (bug 1534310)
            # status_node = self.marionette.find_element(By.CSS_SELECTOR, 'p.showNormal')
            # self.assertEqual(status_node.text,
            #                 self.browser.localize_entity('aboutPrivateBrowsing.notPrivate'),
            #                 'Status text indicates we are not in private browsing mode')

        def window_opener(win):
            with win.marionette.using_context('content'):
                button = self.marionette.find_element(By.ID, 'startPrivateBrowsing')
                button.click()

        pb_window = self.browser.open_window(callback=window_opener,
                                             expected_window_class=BrowserWindow)

        try:
            self.assertTrue(pb_window.is_private)

            def tab_opener(tab):
                with tab.marionette.using_context('content'):
                    link = tab.marionette.find_element(By.ID, 'learnMore')
                    link.click()

            tab = pb_window.tabbar.open_tab(trigger=tab_opener)
            Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                lambda _: tab.location == self.pb_url)

        finally:
            pb_window.close()


class TestAboutPrivateBrowsingWithSearch(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAboutPrivateBrowsingWithSearch, self).setUp()

        # Use a fake local support URL
        support_url = 'about:blank?'
        self.marionette.set_pref('app.support.baseURL', support_url)
        self.pb_url = support_url + 'private-browsing-myths'

        # Enable the search UI
        self.marionette.set_pref('browser.privatebrowsing.searchUI', True)

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
            self.browser.switch_to()

            self.marionette.clear_pref('app.support.baseURL')
            self.marionette.clear_pref('browser.privatebrowsing.searchUI')
        finally:
            super(TestAboutPrivateBrowsingWithSearch, self).tearDown()

    def testCheckAboutPrivateBrowsingWithSearch(self):
        self.assertFalse(self.browser.is_private)

        with self.marionette.using_context('content'):
            self.marionette.navigate('about:privatebrowsing')

            # Disabled awaiting support for Fluent strings in firefox-ui tests (bug 1534310)
            # status_node = self.marionette.find_element(By.CSS_SELECTOR, 'p.showNormal')
            # self.assertEqual(status_node.text,
            #                 self.browser.localize_entity('aboutPrivateBrowsing.notPrivate'),
            #                 'Status text indicates we are not in private browsing mode')

        def window_opener(win):
            with win.marionette.using_context('content'):
                button = self.marionette.find_element(By.ID, 'startPrivateBrowsing')
                button.click()

        pb_window = self.browser.open_window(callback=window_opener,
                                             expected_window_class=BrowserWindow)

        try:
            self.assertTrue(pb_window.is_private)

            # Test the search hand-off
            with self.marionette.using_context('content'):
                search = self.marionette.find_element(By.ID, 'search-handoff-button')
                search.click()

            self.assertTrue(pb_window.navbar.locationbar.focused, 'url bar is focused')

            pb_window.navbar.locationbar.urlbar.send_keys('foo')
            self.assertEqual(pb_window.navbar.locationbar.value, '@google foo',
                             'url bar prepends the @search shortcut')

            # Test the private browsing myths link
            with self.marionette.using_context('content'):
                link = self.marionette.find_element(By.ID, 'private-browsing-myths')
                link.click()
                Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                    lambda _: self.marionette.get_url() == self.pb_url)

        finally:
            pb_window.close()
