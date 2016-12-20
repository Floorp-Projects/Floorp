# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from urlparse import urlparse

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import expected, Wait
from marionette_harness import MarionetteTestCase


class TestNoCertificate(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNoCertificate, self).setUp()

        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.locationbar.identity_popup

        self.url = self.marionette.absolute_url('layout/mozilla.html')

    def tearDown(self):
        try:
            self.browser.switch_to()
            self.identity_popup.close(force=True)
            self.puppeteer.windows.close_all([self.browser])
        finally:
            super(TestNoCertificate, self).tearDown()

    def test_no_certificate(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        # Check the favicon
        # TODO: find a better way to check, e.g., mozmill's isDisplayed
        favicon_hidden = self.marionette.execute_script("""
          return arguments[0].hasAttribute("hidden");
        """, script_args=[self.browser.navbar.locationbar.identity_icon])
        self.assertFalse(favicon_hidden, 'The identity icon is visible')

        # Check that the identity box organization label is blank
        self.assertEqual(self.locationbar.identity_organization_label.get_property('value'), '',
                         'The organization has no label')

        # Open the identity popup
        self.locationbar.open_identity_popup()

        # Check the idenity popup doorhanger
        self.assertEqual(self.identity_popup.element.get_attribute('connection'), 'not-secure')

        # The expander for the security view does not exist
        expected.element_not_present(lambda m: self.identity_popup.main.expander)

        # Only the insecure label is visible
        secure_label = self.identity_popup.view.main.secure_connection_label
        self.assertEqual(secure_label.value_of_css_property('display'), 'none')

        insecure_label = self.identity_popup.view.main.insecure_connection_label
        self.assertNotEqual(insecure_label.value_of_css_property('display'), 'none')

        self.identity_popup.view.main.expander.click()
        Wait(self.marionette).until(lambda _: self.identity_popup.view.security.selected)

        # Open the Page Info window by clicking the "More Information" button
        page_info = self.browser.open_page_info_window(
            lambda _: self.identity_popup.view.security.more_info_button.click())

        # Verify that the current panel is the security panel
        self.assertEqual(page_info.deck.selected_panel, page_info.deck.security)

        # Check the domain listed on the security panel contains the url's host name
        self.assertIn(urlparse(self.url).hostname,
                      page_info.deck.security.domain.get_property('value'))

        # Check the owner label equals localized 'securityNoOwner'
        self.assertEqual(page_info.deck.security.owner.get_property('value'),
                         page_info.localize_property('securityNoOwner'))

        # Check the verifier label equals localized 'notset'
        self.assertEqual(page_info.deck.security.verifier.get_property('value'),
                         page_info.localize_property('notset'))
