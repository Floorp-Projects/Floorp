# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_ui_harness.testcases import FirefoxTestCase


class TestMixedScriptContentBlocking(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.url = 'https://mozqa.com/data/firefox/security/mixed_content_blocked/index.html'

        self.test_elements = [
            ('result1', 'Insecure script one'),
            ('result2', 'Insecure script from iFrame'),
            ('result3', 'Insecure plugin'),
            ('result4', 'Insecure stylesheet'),
        ]

        self.locationbar = self.browser.navbar.locationbar
        self.identity_popup = self.locationbar.identity_popup

    def tearDown(self):
        try:
            self.identity_popup.close(force=True)
        finally:
            FirefoxTestCase.tearDown(self)

    def _expect_protection_status(self, enabled):
        if enabled:
            color, icon_filename, state = (
                'rgb(0, 136, 0)',
                'identity-mixed-active-blocked',
                'blocked'
            )
        else:
            color, icon_filename, state = (
                'rgb(255, 0, 0)',
                'identity-mixed-active-loaded',
                'unblocked'
            )

        # First call to Wait() needs a longer timeout due to the reload of the web page.
        connection_icon = self.locationbar.connection_icon
        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda _: icon_filename in connection_icon.value_of_css_property('list-style-image'),
            message="The correct icon is displayed"
        )

        with self.marionette.using_context('content'):
            for identifier, description in self.test_elements:
                el = self.marionette.find_element(By.ID, identifier)
                Wait(self.marionette).until(
                    lambda mn: el.value_of_css_property('color') == color,
                    message=("%s has been %s" % (description, state))
                )

    def expect_protection_enabled(self):
        self._expect_protection_status(True)

    def expect_protection_disabled(self):
        self._expect_protection_status(False)

    def test_mixed_content_page(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        self.expect_protection_enabled()

        # Disable mixed content blocking via identity popup
        self.locationbar.open_identity_popup()
        self.identity_popup.view.main.expander.click()
        Wait(self.marionette).until(lambda _: self.identity_popup.view.security.selected)

        disable_button = self.identity_popup.view.security.disable_mixed_content_blocking_button
        disable_button.click()

        self.expect_protection_disabled()

        # A reload keeps blocking disabled
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

        self.expect_protection_disabled()
