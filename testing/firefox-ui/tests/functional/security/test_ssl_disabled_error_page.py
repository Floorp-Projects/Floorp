# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import By, expected, Wait
from marionette_driver.errors import MarionetteException
from marionette_harness import MarionetteTestCase


class TestSSLDisabledErrorPage(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSSLDisabledErrorPage, self).setUp()

        self.url = 'https://tlsv1-0.mozqa.com'

        self.puppeteer.utils.sanitize({"sessions": True})

        # Disable SSL 3.0, TLS 1.0 and TLS 1.1 for secure connections
        # by forcing the use of TLS 1.2
        # see: http://kb.mozillazine.org/Security.tls.version.*#Possible_values_and_their_effects
        self.puppeteer.prefs.set_pref('security.tls.version.min', 3)
        self.puppeteer.prefs.set_pref('security.tls.version.max', 3)

    def tearDown(self):
        try:
            self.marionette.clear_pref('security.tls.version.min')
            self.marionette.clear_pref('security.tls.version.max')
        finally:
            super(TestSSLDisabledErrorPage, self).tearDown()

    def test_ssl_disabled_error_page(self):
        with self.marionette.using_context('content'):
            # Open the test page
            self.assertRaises(MarionetteException, self.marionette.navigate, self.url)

            # Wait for the DOM to receive events
            time.sleep(1)

            # Verify "Secure Connection Failed" error page title
            title = self.marionette.find_element(By.CLASS_NAME, 'title-text')
            nss_failure2title = self.browser.get_entity('nssFailure2.title')
            self.assertEquals(title.get_property('textContent'), nss_failure2title)

            # Verify the error message is correct
            short_description = self.marionette.find_element(By.ID, 'errorShortDescText')
            self.assertIn('SSL_ERROR_UNSUPPORTED_VERSION',
                          short_description.get_property('textContent'))
            self.assertIn('mozqa.com', short_description.get_property('textContent'))

            # Verify that the "Restore" button appears and works
            reset_button = self.marionette.find_element(By.ID, 'prefResetButton')
            reset_button.click()

            # With the preferences reset, the page has to load correctly
            Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                expected.element_present(By.LINK_TEXT, 'http://quality.mozilla.org'))
