# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from marionette_driver import By
from marionette_driver.errors import MarionetteException

from firefox_puppeteer.testcases import FirefoxTestCase


class TestUnknownIssuer(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.url = 'https://ssl-unknownissuer.mozqa.com'

    def test_unknown_issuer(self):
        with self.marionette.using_context('content'):
            # Go to a site that has a cert with an unknown issuer
            self.assertRaises(MarionetteException, self.marionette.navigate, self.url)

            # Wait for the DOM to receive events
            time.sleep(1)

            # Check the link in cert_domain_link
            link = self.marionette.find_element(By.ID, 'cert_domain_link')
            self.assertEquals(link.get_attribute('textContent'),
                              'ssl-selfsigned-unknownissuer.mozqa.com')

            # Verify the "Go Back" and "Advanced" buttons appear
            self.assertIsNotNone(self.marionette.find_element(By.ID, 'returnButton'))
            self.assertIsNotNone(self.marionette.find_element(By.ID, 'advancedButton'))

            # Verify the error code is correct
            text = self.marionette.find_element(By.ID, 'technicalContentText')
            self.assertIn('SEC_ERROR_UNKNOWN_ISSUER', text.get_attribute('textContent'))
