# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import Wait
from marionette_harness import MarionetteTestCase, skip_if_e10s


class TestSSLStatusAfterRestart(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSSLStatusAfterRestart, self).setUp()

        self.test_data = (
            {
                'url': 'https://ssl-dv.mozqa.com',
                'identity': '',
                'type': 'secure'
            },
            {
                'url': 'https://ssl-ev.mozqa.com/',
                'identity': 'Mozilla Corporation',
                'type': 'secure-ev'
            },
            {
                'url': 'https://ssl-ov.mozqa.com/',
                'identity': '',
                'type': 'secure'
            }
        )

        # Set browser to restore previous session
        self.marionette.set_pref('browser.startup.page', 3)

        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.locationbar.identity_popup

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
            self.browser.switch_to()
            self.identity_popup.close(force=True)
            self.marionette.clear_pref('browser.startup.page')
        finally:
            super(TestSSLStatusAfterRestart, self).tearDown()

    @skip_if_e10s("Bug 1325047")
    def test_ssl_status_after_restart(self):
        for item in self.test_data:
            with self.marionette.using_context('content'):
                self.marionette.navigate(item['url'])
            self.verify_certificate_status(item)
            self.browser.tabbar.open_tab()

        self.restart()

        # Refresh references to elements
        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.locationbar.identity_popup

        for index, item in enumerate(self.test_data):
            self.browser.tabbar.tabs[index].select()
            self.verify_certificate_status(item)

    def verify_certificate_status(self, item):
        url, identity, cert_type = item['url'], item['identity'], item['type']

        # Check the favicon
        # TODO: find a better way to check, e.g., mozmill's isDisplayed
        favicon_hidden = self.marionette.execute_script("""
          return arguments[0].hasAttribute("hidden");
        """, script_args=[self.browser.navbar.locationbar.identity_icon])
        self.assertFalse(favicon_hidden)

        self.locationbar.open_identity_popup()

        # Check the type shown on the identity popup doorhanger
        self.assertEqual(self.identity_popup.element.get_attribute('connection'),
                         cert_type)

        self.identity_popup.view.main.expander.click()
        Wait(self.marionette).until(lambda _: self.identity_popup.view.security.selected)

        # Check the identity label
        self.assertEqual(self.locationbar.identity_organization_label.get_property('value'),
                         identity)

        # Get the information from the certificate
        cert = self.browser.tabbar.selected_tab.certificate

        # Open the Page Info window by clicking the More Information button
        page_info = self.browser.open_page_info_window(
            lambda _: self.identity_popup.view.security.more_info_button.click())

        # Verify that the current panel is the security panel
        self.assertEqual(page_info.deck.selected_panel, page_info.deck.security)

        # Verify the domain listed on the security panel
        # If this is a wildcard cert, check only the domain
        if cert['commonName'].startswith('*'):
            self.assertIn(self.puppeteer.security.get_domain_from_common_name(cert['commonName']),
                          page_info.deck.security.domain.get_property('value'),
                          'Expected domain found in certificate for ' + url)
        else:
            self.assertEqual(page_info.deck.security.domain.get_property('value'),
                             cert['commonName'],
                             'Domain value matches certificate common name.')

        # Verify the owner listed on the security panel
        if identity != '':
            owner = cert['organization']
        else:
            owner = page_info.localize_property('securityNoOwner')

        self.assertEqual(page_info.deck.security.owner.get_property('value'), owner,
                         'Expected owner label found for ' + url)

        # Verify the verifier listed on the security panel
        self.assertEqual(page_info.deck.security.verifier.get_property('value'),
                         cert['issuerOrganization'],
                         'Verifier matches issuer of certificate for ' + url)
        page_info.close()
