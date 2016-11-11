# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from firefox_puppeteer import PuppeteerMixin
from marionette import MarionetteTestCase
from marionette_driver import By, expected, Wait


class TestSafeBrowsingWarningPages(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSafeBrowsingWarningPages, self).setUp()

        self.urls = [
            # Unwanted software URL
            'https://www.itisatrap.org/firefox/unwanted.html',
            # Phishing URL
            'https://www.itisatrap.org/firefox/its-a-trap.html',
            # Malware URL
            'https://www.itisatrap.org/firefox/its-an-attack.html'
        ]

        self.puppeteer.prefs.set_pref('browser.safebrowsing.phishing.enabled', True)
        self.puppeteer.prefs.set_pref('browser.safebrowsing.malware.enabled', True)

        # Give the browser a little time, because SafeBrowsing.jsm takes a
        # while between start up and adding the example urls to the db.
        # hg.mozilla.org/mozilla-central/file/46aebcd9481e/browser/base/content/browser.js#l1194
        time.sleep(3)

        # TODO: Bug 1139544: While we don't have a reliable way to close the safe browsing
        # notification bar when a test fails, run this test in a new tab.
        self.browser.tabbar.open_tab()

    def tearDown(self):
        try:
            self.puppeteer.utils.permissions.remove('https://www.itisatrap.org', 'safe-browsing')
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
            self.marionette.clear_pref('browser.safebrowsing.malware.enabled')
            self.marionette.clear_pref('browser.safebrowsing.phishing.enabled')
        finally:
            super(TestSafeBrowsingWarningPages, self).tearDown()

    def test_warning_pages(self):
        with self.marionette.using_context("content"):
            for unsafe_page in self.urls:
                # Load a test page, then test the get me out button
                self.marionette.navigate(unsafe_page)
                # Wait for the DOM to receive events for about:blocked
                time.sleep(1)
                self.check_get_me_out_of_here_button(unsafe_page)

                # Load the test page again, then test the report button
                self.marionette.navigate(unsafe_page)
                # Wait for the DOM to receive events for about:blocked
                time.sleep(1)
                self.check_report_button(unsafe_page)

                # Load the test page again, then test the ignore warning button
                self.marionette.navigate(unsafe_page)
                # Wait for the DOM to receive events for about:blocked
                time.sleep(1)
                self.check_ignore_warning_button(unsafe_page)

    def check_get_me_out_of_here_button(self, unsafe_page):
        button = self.marionette.find_element(By.ID, "getMeOutButton")
        button.click()

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda mn: self.browser.default_homepage in mn.get_url())

    def check_report_button(self, unsafe_page):
        # Get the URL of the support site for phishing and malware. This may result in a redirect.
        with self.marionette.using_context('chrome'):
            url = self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/Services.jsm");
              return Services.urlFormatter.formatURLPref("app.support.baseURL")
                                                         + "phishing-malware";
            """)

        button = self.marionette.find_element(By.ID, "reportButton")
        button.click()

        # Wait for the button to become stale, whereby a longer timeout is needed
        # here to not fail in case of slow connections.
        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            expected.element_stale(button))

        # Wait for page load to be completed, so we can verify the URL even if a redirect happens.
        # TODO: Bug 1140470: use replacement for mozmill's waitforPageLoad
        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda mn: mn.execute_script('return document.readyState == "DOMContentLoaded" ||'
                                         '       document.readyState == "complete";')
        )

        # check that our current url matches the final url we expect
        self.assertEquals(self.marionette.get_url(), self.browser.get_final_url(url))

    def check_ignore_warning_button(self, unsafe_page):
        button = self.marionette.find_element(By.ID, 'ignoreWarningButton')
        button.click()

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            expected.element_present(By.ID, 'main-feature'))
        self.assertEquals(self.marionette.get_url(), self.browser.get_final_url(unsafe_page))

        # Clean up by removing safe browsing permission for unsafe page
        self.puppeteer.utils.permissions.remove('https://www.itisatrap.org', 'safe-browsing')
