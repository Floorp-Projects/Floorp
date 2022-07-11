# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import time

from marionette_driver import By, expected, Wait
from marionette_harness import MarionetteTestCase, WindowManagerMixin


class TestSafeBrowsingWarningPages(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestSafeBrowsingWarningPages, self).setUp()

        self.urls = [
            # Unwanted software URL
            "https://www.itisatrap.org/firefox/unwanted.html",
            # Phishing URL
            "https://www.itisatrap.org/firefox/its-a-trap.html",
            # Malware URL
            "https://www.itisatrap.org/firefox/its-an-attack.html",
        ]

        self.default_homepage = self.marionette.get_pref("browser.startup.homepage")
        self.support_page = self.marionette.absolute_url("support.html?topic=")

        self.marionette.set_pref("app.support.baseURL", self.support_page)
        self.marionette.set_pref("browser.safebrowsing.phishing.enabled", True)
        self.marionette.set_pref("browser.safebrowsing.malware.enabled", True)

        # Give the browser a little time, because SafeBrowsing.jsm takes a
        # while between start up and adding the example urls to the db.
        # hg.mozilla.org/mozilla-central/file/46aebcd9481e/browser/base/content/browser.js#l1194
        time.sleep(3)

        # Run this test in a new tab.
        new_tab = self.open_tab()
        self.marionette.switch_to_window(new_tab)

    def tearDown(self):
        try:
            self.marionette.clear_pref("app.support.baseURL")
            self.marionette.clear_pref("browser.safebrowsing.malware.enabled")
            self.marionette.clear_pref("browser.safebrowsing.phishing.enabled")

            self.remove_permission("https://www.itisatrap.org", "safe-browsing")
            self.close_all_tabs()
        finally:
            super(TestSafeBrowsingWarningPages, self).tearDown()

    def test_warning_pages(self):
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
            self.check_report_link(unsafe_page)

            # Load the test page again, then test the ignore warning button
            self.marionette.navigate(unsafe_page)
            # Wait for the DOM to receive events for about:blocked
            time.sleep(1)
            self.check_ignore_warning_button(unsafe_page)

    def get_final_url(self, url):
        self.marionette.navigate(url)
        return self.marionette.get_url()

    def remove_permission(self, host, permission):
        with self.marionette.using_context("chrome"):
            self.marionette.execute_script(
                """
              let uri = Services.io.newURI(arguments[0], null, null);
              let principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});
              Services.perms.removeFromPrincipal(principal, arguments[1]);
            """,
                script_args=[host, permission],
            )

    def check_get_me_out_of_here_button(self, unsafe_page):
        button = self.marionette.find_element(By.ID, "goBackButton")
        button.click()

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: self.default_homepage in mn.get_url()
        )

    def check_report_link(self, unsafe_page):
        # Get the URL of the support site for phishing and malware. This may result in a redirect.
        with self.marionette.using_context("chrome"):
            url = self.marionette.execute_script(
                """
              return Services.urlFormatter.formatURLPref("app.support.baseURL")
                                                         + "phishing-malware";
            """
            )

        button = self.marionette.find_element(By.ID, "seeDetailsButton")
        button.click()
        link = self.marionette.find_element(By.ID, "firefox_support")
        link.click()

        # Wait for the button to become stale, whereby a longer timeout is needed
        # here to not fail in case of slow connections.
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_stale(button)
        )

        # Wait for page load to be completed, so we can verify the URL even if a redirect happens.
        # TODO: Bug 1140470: use replacement for mozmill's waitforPageLoad
        expected_url = self.get_final_url(url)
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: expected_url == mn.get_url(),
            message="The expected URL '{}' has not been loaded".format(expected_url),
        )

        topic = self.marionette.find_element(By.ID, "topic")
        self.assertEqual(topic.text, "phishing-malware")

    def check_ignore_warning_button(self, unsafe_page):
        button = self.marionette.find_element(By.ID, "seeDetailsButton")
        button.click()
        link = self.marionette.find_element(By.ID, "ignore_warning_link")
        link.click()

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_present(By.ID, "main-feature")
        )
        self.assertEqual(self.marionette.get_url(), self.get_final_url(unsafe_page))

        # Clean up by removing safe browsing permission for unsafe page
        self.remove_permission("https://www.itisatrap.org", "safe-browsing")
