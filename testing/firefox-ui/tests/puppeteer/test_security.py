# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_ui_harness.testcases import FirefoxTestCase

from firefox_puppeteer.errors import NoCertificateError


class TestSecurity(FirefoxTestCase):

    def test_get_address_from_certificate(self):
        url = 'https://ssl-ev.mozqa.com'

        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate(url)

        cert = self.browser.tabbar.tabs[0].certificate
        self.assertIn(cert['commonName'], url)
        self.assertEqual(cert['organization'], 'Mozilla Corporation')
        self.assertEqual(cert['issuerOrganization'], 'DigiCert Inc')

        address = self.puppeteer.security.get_address_from_certificate(cert)
        self.assertIsNotNone(address)
        self.assertIsNotNone(address['city'])
        self.assertIsNotNone(address['country'])
        self.assertIsNotNone(address['postal_code'])
        self.assertIsNotNone(address['state'])
        self.assertIsNotNone(address['street'])

    def test_get_certificate(self):
        url_http = self.marionette.absolute_url('layout/mozilla.html')
        url_https = 'https://ssl-ev.mozqa.com'

        # Test EV certificate
        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate(url_https)
        cert = self.browser.tabbar.tabs[0].certificate
        self.assertIn(cert['commonName'], url_https)

        # HTTP connections do not have a SSL certificate
        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate(url_http)
        with self.assertRaises(NoCertificateError):
            self.browser.tabbar.tabs[0].certificate
