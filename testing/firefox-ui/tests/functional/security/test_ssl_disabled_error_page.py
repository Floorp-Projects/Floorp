# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from marionette_driver import By
from marionette_driver.errors import MarionetteException

from firefox_ui_harness.testcases import FirefoxTestCase


class TestSSLDisabledErrorPage(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.url = 'https://tlsv1-0.mozqa.com'

        self.utils.sanitize({"sessions": True})

        # Disable SSL 3.0, TLS 1.0 and TLS 1.1 for secure connections
        # by forcing the use of TLS 1.2
        # see: http://kb.mozillazine.org/Security.tls.version.*#Possible_values_and_their_effects
        self.prefs.set_pref('security.tls.version.min', 3)
        self.prefs.set_pref('security.tls.version.max', 3)

    def test_ssl_disabled_error_page(self):
        with self.marionette.using_context('content'):
            # Open the test page
            self.assertRaises(MarionetteException, self.marionette.navigate, self.url)

            # Wait for the DOM to receive events
            time.sleep(1)

            # Verify "Secure Connection Failed" error page title
            title = self.marionette.find_element(By.ID, 'errorTitleText')
            nss_failure2title = self.browser.get_entity('nssFailure2.title')
            self.assertEquals(title.get_attribute('textContent'), nss_failure2title)

            # Verify "Try Again" button appears
            try_again_button = self.marionette.find_element(By.ID, 'errorTryAgain')
            self.assertTrue(try_again_button.is_displayed())

            # Verify the error message is correct
            short_description = self.marionette.find_element(By.ID, 'errorShortDescText')
            self.assertIn('SSL_ERROR_UNSUPPORTED_VERSION',
                          short_description.get_attribute('textContent'))
            self.assertIn('mozqa.com', short_description.get_attribute('textContent'))
