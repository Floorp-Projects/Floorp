# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from marionette_driver import By, expected, Wait

from firefox_ui_harness.testcases import FirefoxTestCase


class TestSafeBrowsingNotificationBar(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.test_data = [
            # Unwanted software URL
            {
                # First two properties are not needed,
                # since these errors are not reported
                'button_property': None,
                'report_page': None,
                'unsafe_page': 'https://www.itisatrap.org/firefox/unwanted.html'
            },
            # Phishing URL info
            {
                'button_property': 'safebrowsing.notADeceptiveSiteButton.label',
                'report_page': 'google.com/safebrowsing/report_error',
                'unsafe_page': 'https://www.itisatrap.org/firefox/its-a-trap.html'
            },
            # Malware URL object
            {
                'button_property': 'safebrowsing.notAnAttackButton.label',
                'report_page': 'stopbadware.org',
                'unsafe_page': 'https://www.itisatrap.org/firefox/its-an-attack.html'
            }
        ]

        self.prefs.set_pref('browser.safebrowsing.phishing.enabled', True)
        self.prefs.set_pref('browser.safebrowsing.malware.enabled', True)

        # Give the browser a little time, because SafeBrowsing.jsm takes a while
        # between start up and adding the example urls to the db.
        # hg.mozilla.org/mozilla-central/file/46aebcd9481e/browser/base/content/browser.js#l1194
        time.sleep(3)

        # TODO: Bug 1139544: While we don't have a reliable way to close the safe browsing
        # notification bar when a test fails, run this test in a new tab.
        self.browser.tabbar.open_tab()

    def tearDown(self):
        try:
            self.utils.permissions.remove('https://www.itisatrap.org', 'safe-browsing')
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
            self.marionette.clear_pref('browser.safebrowsing.phishing.enabled')
            self.marionette.clear_pref('browser.safebrowsing.malware.enabled')
        finally:
            FirefoxTestCase.tearDown(self)

    def test_notification_bar(self):
        with self.marionette.using_context('content'):
            for item in self.test_data:
                button_property = item['button_property']
                report_page, unsafe_page = item['report_page'], item['unsafe_page']

                # Navigate to the unsafe page
                # Check "ignore warning" link then notification bar's "not badware" button
                # Only do this if feature supports it
                if button_property is not None:
                    self.marionette.navigate(unsafe_page)
                    # Wait for the DOM to receive events for about:blocked
                    time.sleep(1)
                    self.check_ignore_warning_button(unsafe_page)
                    self.check_not_badware_button(button_property, report_page)

                # Return to the unsafe page
                # Check "ignore warning" link then notification bar's "get me out" button
                self.marionette.navigate(unsafe_page)
                # Wait for the DOM to receive events for about:blocked
                time.sleep(1)
                self.check_ignore_warning_button(unsafe_page)
                self.check_get_me_out_of_here_button()

                # Return to the unsafe page
                # Check "ignore warning" link then notification bar's "X" button
                self.marionette.navigate(unsafe_page)
                # Wait for the DOM to receive events for about:blocked
                time.sleep(1)
                self.check_ignore_warning_button(unsafe_page)
                self.check_x_button()

    def check_ignore_warning_button(self, unsafe_page):
        button = self.marionette.find_element(By.ID, 'ignoreWarningButton')
        button.click()

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            expected.element_present(By.ID, 'main-feature'),
            message='Expected target element "#main-feature" has not been found',
        )
        self.assertEquals(self.marionette.get_url(), self.browser.get_final_url(unsafe_page))

        # Clean up here since the permission gets set in this function
        self.utils.permissions.remove('https://www.itisatrap.org', 'safe-browsing')

    # Check the not a forgery or attack button in the notification bar
    def check_not_badware_button(self, button_property, report_page):
        with self.marionette.using_context('chrome'):
            # TODO: update to use safe browsing notification bar class when bug 1139544 lands
            label = self.browser.get_property(button_property)
            button = (self.marionette.find_element(By.ID, 'content')
                      .find_element('anon attribute', {'label': label}))

            self.browser.tabbar.open_tab(lambda _: button.click())

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda mn: report_page in mn.get_url(),
            message='The expected safe-browsing report page has not been opened',
        )

        with self.marionette.using_context('chrome'):
            self.browser.tabbar.close_tab()

    def check_get_me_out_of_here_button(self):
        with self.marionette.using_context('chrome'):
            # TODO: update to use safe browsing notification bar class when bug 1139544 lands
            label = self.browser.get_property('safebrowsing.getMeOutOfHereButton.label')
            button = (self.marionette.find_element(By.ID, 'content')
                      .find_element('anon attribute', {'label': label}))
            button.click()

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda mn: self.browser.default_homepage in mn.get_url(),
            message='The default home page has not been loaded',
        )

    def check_x_button(self):
        with self.marionette.using_context('chrome'):
            # TODO: update to use safe browsing notification bar class when bug 1139544 lands
            button = (self.marionette.find_element(By.ID, 'content')
                      .find_element('anon attribute', {'value': 'blocked-badware-page'})
                      .find_element('anon attribute',
                                    {'class': 'messageCloseButton close-icon tabbable'}))
            button.click()

            Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
                expected.element_stale(button),
                message='The notification bar has not been closed',
            )
