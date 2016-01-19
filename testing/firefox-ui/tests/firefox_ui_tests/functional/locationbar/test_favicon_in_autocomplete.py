# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait

from firefox_ui_harness.decorators import skip_under_xvfb
from firefox_puppeteer.testcases import FirefoxTestCase


class TestFaviconInAutocomplete(FirefoxTestCase):

    PREF_SUGGEST_SEARCHES = 'browser.urlbar.suggest.searches'

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Disable search suggestions to get results only for history and bookmarks
        self.prefs.set_pref(self.PREF_SUGGEST_SEARCHES, False)

        self.places.remove_all_history()

        self.test_urls = [self.marionette.absolute_url('layout/mozilla.html')]

        self.test_string = 'mozilla'
        self.test_favicon = 'mozilla_favicon.ico'

        self.autocomplete_results = self.browser.navbar.locationbar.autocomplete_results

    def tearDown(self):
        try:
            self.autocomplete_results.close(force=True)
        finally:
            FirefoxTestCase.tearDown(self)

    @skip_under_xvfb
    def test_favicon_in_autocomplete(self):
        # Open the test page
        def load_urls():
            with self.marionette.using_context('content'):
                self.marionette.navigate(self.test_urls[0])
        self.places.wait_for_visited(self.test_urls, load_urls)

        locationbar = self.browser.navbar.locationbar

        # Clear the location bar, type the test string, check that autocomplete list opens
        locationbar.clear()
        locationbar.urlbar.send_keys(self.test_string)
        self.assertEqual(locationbar.value, self.test_string)
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_complete)

        result = self.autocomplete_results.visible_results[1]

        result_icon = self.marionette.execute_script("""
          return arguments[0].image;
        """, script_args=[result])

        self.assertIn(self.test_favicon, result_icon)

        self.autocomplete_results.close()
