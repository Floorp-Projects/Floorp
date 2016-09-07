# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_ui_harness.testcases import FirefoxTestCase

from firefox_puppeteer.ui.browser.window import BrowserWindow


class TestAboutPrivateBrowsing(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Use a fake local support URL
        support_url = 'about:blank?'
        self.prefs.set_pref('app.support.baseURL', support_url)

        self.pb_url = support_url + 'private-browsing'

    def tearDown(self):
        try:
            self.marionette.clear_pref('app.support.baseURL')
        finally:
            FirefoxTestCase.tearDown(self)

    def testCheckAboutPrivateBrowsing(self):
        self.assertFalse(self.browser.is_private)

        with self.marionette.using_context('content'):
            self.marionette.navigate('about:privatebrowsing')

            status_node = self.marionette.find_element(By.CSS_SELECTOR, 'p.showNormal')
            self.assertEqual(status_node.text,
                             self.browser.get_entity('aboutPrivateBrowsing.notPrivate'),
                             'Status text indicates we are not in private browsing mode')

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
            Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
                lambda _: tab.location == self.pb_url)

        finally:
            pb_window.close()
