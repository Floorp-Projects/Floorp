# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from firefox_puppeteer import PuppeteerMixin
from marionette import MarionetteTestCase
from marionette_driver import By, Wait
from marionette_driver.errors import MarionetteException


class TestUntrustedConnectionErrorPage(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestUntrustedConnectionErrorPage, self).setUp()

        self.url = 'https://ssl-selfsigned.mozqa.com'

    def test_untrusted_connection_error_page(self):
        self.marionette.set_context('content')

        # In some localized builds, the default page redirects
        target_url = self.browser.get_final_url(self.browser.default_homepage)

        self.assertRaises(MarionetteException, self.marionette.navigate, self.url)

        # Wait for the DOM to receive events
        time.sleep(1)

        button = self.marionette.find_element(By.ID, "returnButton")
        button.click()

        Wait(self.marionette, timeout=self.browser.timeout_page_load).until(
            lambda mn: target_url == self.marionette.get_url())
