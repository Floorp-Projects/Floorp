# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_ui_harness.testcases import FirefoxTestCase


class TestSanitize(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        # Clear all previous history and cookies.
        self.puppeteer.places.remove_all_history()
        self.marionette.delete_all_cookies()

        self.urls = [
            'layout/mozilla_projects.html',
            'layout/mozilla.html',
            'layout/mozilla_mission.html',
            'cookies/cookie_single.html'
        ]
        self.urls = [self.marionette.absolute_url(url) for url in self.urls]

        # Open the test urls, including the single cookie setting page.
        def load_urls():
            with self.marionette.using_context('content'):
                for url in self.urls:
                    self.marionette.navigate(url)
        self.puppeteer.places.wait_for_visited(self.urls, load_urls)

    def tearDown(self):
        FirefoxTestCase.tearDown(self)

    def test_sanitize_history(self):
        """ Clears history. """
        self.assertEqual(self.puppeteer.places.get_all_urls_in_history(), self.urls)
        self.puppeteer.utils.sanitize(data_type={"history": True})
        self.assertEqual(self.puppeteer.places.get_all_urls_in_history(), [])

    def test_sanitize_cookies(self):
        """ Clears cookies. """
        self.assertIsNotNone(self.marionette.get_cookie('litmus_1'))
        self.puppeteer.utils.sanitize(data_type={"cookies": True})
        self.assertIsNone(self.marionette.get_cookie('litmus_1'))
