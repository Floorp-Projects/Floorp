# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait

from firefox_ui_harness.testcases import FirefoxTestCase


class TestAccessLocationBar(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Clear complete history so there's no interference from previous entries.
        self.places.remove_all_history()

        self.test_urls = [
            'layout/mozilla_projects.html',
            'layout/mozilla.html',
            'layout/mozilla_mission.html'
        ]
        self.test_urls = [self.marionette.absolute_url(t)
                          for t in self.test_urls]

        self.locationbar = self.browser.navbar.locationbar
        self.autocomplete_results = self.locationbar.autocomplete_results
        self.urlbar = self.locationbar.urlbar

    def test_access_locationbar_history(self):

        # Open some local pages, then about:blank
        def load_urls():
            with self.marionette.using_context('content'):
                for url in self.test_urls:
                    self.marionette.navigate(url)
        self.places.wait_for_visited(self.test_urls, load_urls)
        with self.marionette.using_context('content'):
            self.marionette.navigate('about:blank')

        # Need to blur url bar or autocomplete won't load - bug 1038614
        self.marionette.execute_script("""arguments[0].blur();""", script_args=[self.urlbar])

        # Clear contents of url bar to focus, then arrow down for list of visited sites
        # Verify that autocomplete is open and results are displayed
        self.locationbar.clear()
        self.urlbar.send_keys(self.keys.ARROW_DOWN)
        Wait(self.marionette).until(lambda _: self.autocomplete_results.is_open)
        Wait(self.marionette).until(lambda _: len(self.autocomplete_results.visible_results) > 1)

        # Arrow down again to select first item in list, appearing in reversed order, as loaded.
        # Verify first item.
        self.urlbar.send_keys(self.keys.ARROW_DOWN)
        Wait(self.marionette).until(lambda _: self.autocomplete_results.selected_index == '0')
        self.assertIn('mission', self.locationbar.value)

        # Navigate to the currently selected url
        # Verify it loads by comparing the page url to the test url
        self.urlbar.send_keys(self.keys.ENTER)
        self.assertEqual(self.locationbar.value, self.test_urls[-1])
