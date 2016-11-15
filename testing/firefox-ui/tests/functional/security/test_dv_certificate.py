# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette import MarionetteTestCase
from marionette_driver import Wait


class TestDVCertificate(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestDVCertificate, self).setUp()

        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.browser.navbar.locationbar.identity_popup

        self.url = 'https://ssl-dv.mozqa.com'

    def tearDown(self):
        try:
            self.browser.switch_to()
            self.identity_popup.close(force=True)
            self.puppeteer.windows.close_all([self.browser])
        finally:
            super(TestDVCertificate, self).tearDown()

    def test_dv_cert(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        self.assertEqual(self.locationbar.identity_box.get_attribute('className'),
                         'verifiedDomain')

        # Open the identity popup
        self.locationbar.open_identity_popup()

        # Check the identity popup doorhanger
        self.assertEqual(self.identity_popup.element.get_attribute('connection'), 'secure')

        cert = self.browser.tabbar.selected_tab.certificate

        # The shown host equals to the certificate
        self.assertEqual(self.identity_popup.view.main.host.get_attribute('textContent'),
                         cert['commonName'])

        # Only the secure label is visible in the main view
        secure_label = self.identity_popup.view.main.secure_connection_label
        self.assertNotEqual(secure_label.value_of_css_property('display'), 'none')

        insecure_label = self.identity_popup.view.main.insecure_connection_label
        self.assertEqual(insecure_label.value_of_css_property('display'), 'none')

        self.identity_popup.view.main.expander.click()
        Wait(self.marionette).until(lambda _: self.identity_popup.view.security.selected)

        # Only the secure label is visible in the security view
        secure_label = self.identity_popup.view.security.secure_connection_label
        self.assertNotEqual(secure_label.value_of_css_property('display'), 'none')

        insecure_label = self.identity_popup.view.security.insecure_connection_label
        self.assertEqual(insecure_label.value_of_css_property('display'), 'none')

        verifier_label = self.browser.get_property('identity.identified.verifier')
        self.assertEqual(self.identity_popup.view.security.verifier.get_attribute('textContent'),
                         verifier_label.replace("%S", cert['issuerOrganization']))

        def opener(mn):
            self.identity_popup.view.security.more_info_button.click()

        page_info_window = self.browser.open_page_info_window(opener)
        deck = page_info_window.deck

        self.assertEqual(deck.selected_panel, deck.security)

        self.assertEqual(deck.security.domain.get_attribute('value'),
                         cert['commonName'])

        self.assertEqual(deck.security.owner.get_attribute('value'),
                         page_info_window.get_property('securityNoOwner'))

        self.assertEqual(deck.security.verifier.get_attribute('value'),
                         cert['issuerOrganization'])
