# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class TestEscapeAutocomplete(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestEscapeAutocomplete, self).setUp()

        # Clear complete history so there's no interference from previous entries.
        self.puppeteer.places.remove_all_history()

        self.test_urls = [
            'layout/mozilla.html',
            'layout/mozilla_community.html',
        ]
        self.test_urls = [self.marionette.absolute_url(t)
                          for t in self.test_urls]

        self.test_string = 'mozilla'

        self.locationbar = self.browser.navbar.locationbar
        self.autocomplete_results = self.locationbar.autocomplete_results

    def tearDown(self):
        self.autocomplete_results.close(force=True)

        super(TestEscapeAutocomplete, self).tearDown()

    def test_escape_autocomplete(self):
        # Open some local pages
        def load_urls():
            with self.marionette.using_context('content'):
                for url in self.test_urls:
                    self.marionette.navigate(url)
        self.puppeteer.places.wait_for_visited(self.test_urls, load_urls)

        # Clear the location bar, type the test string, check that autocomplete list opens
        self.locationbar.clear()
        self.locationbar.urlbar.send_keys(self.test_string)
        self.assertEqual(self.locationbar.value, self.test_string)
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_open)

        # Press escape, check location bar value, check autocomplete list closed
        self.locationbar.urlbar.send_keys(self.puppeteer.keys.ESCAPE)
        self.assertEqual(self.locationbar.value, self.test_string)
        Wait(self.marionette).until(lambda _: not self.autocomplete_results.is_open)

        # Press escape again and check that locationbar returns to the page url
        self.locationbar.urlbar.send_keys(self.puppeteer.keys.ESCAPE)
        self.assertEqual(self.locationbar.value, self.test_urls[-1])
