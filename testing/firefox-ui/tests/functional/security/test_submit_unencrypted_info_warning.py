# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import By, expected, Wait
from marionette_driver.errors import NoAlertPresentException
from marionette_driver.marionette import Alert
from marionette_harness import MarionetteTestCase


class TestSubmitUnencryptedInfoWarning(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestSubmitUnencryptedInfoWarning, self).setUp()

        self.url = 'https://ssl-dv.mozqa.com/data/firefox/security/unencryptedsearch.html'
        self.test_string = 'mozilla'

        # Disable rcwn to make cache behavior deterministic
        self.marionette.set_pref('network.http.rcwn.enabled', False)
        self.marionette.set_pref('security.warn_submit_insecure', True)

    def tearDown(self):
        try:
            self.marionette.clear_pref('network.http.rcwn.enabled')
            self.marionette.clear_pref('security.warn_submit_insecure')
        finally:
            super(TestSubmitUnencryptedInfoWarning, self).tearDown()

    def test_submit_unencrypted_info_warning(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

            # Get the page's search box and submit button.
            searchbox = self.marionette.find_element(By.ID, 'q')
            button = self.marionette.find_element(By.ID, 'submit')

            # Use the page's search box to submit information.
            searchbox.send_keys(self.test_string)
            button.click()

            # Get the expected warning text and replace its two instances of "##" with "\n\n".
            message = self.browser.localize_property('formPostSecureToInsecureWarning.message')
            message = message.replace('##', '\n\n')

            # Wait for the warning, verify the expected text matches warning, accept the warning
            warning = Alert(self.marionette)
            try:
                Wait(self.marionette,
                     ignored_exceptions=NoAlertPresentException,
                     timeout=self.marionette.timeout.page_load).until(
                    lambda _: warning.text == message)
            finally:
                warning.accept()

            # Wait for the search box to become stale, then wait for the page to be reloaded.
            Wait(self.marionette).until(expected.element_stale(searchbox))

            # TODO: Bug 1140470: use replacement for mozmill's waitforPageLoad
            Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                lambda mn: mn.execute_script('return document.readyState == "DOMContentLoaded" ||'
                                             '       document.readyState == "complete";')
            )

            # Check that search_term contains the test string.
            search_term = self.marionette.find_element(By.ID, 'search-term')
            self.assertEqual(search_term.get_property('textContent'), self.test_string)
