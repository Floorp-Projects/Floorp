# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_ui_harness.decorators import skip_under_xvfb
from firefox_puppeteer.testcases import FirefoxTestCase


class TestStarInAutocomplete(FirefoxTestCase):
    """ This replaces
    http://hg.mozilla.org/qa/mozmill-tests/file/default/firefox/tests/functional/testAwesomeBar/testSuggestBookmarks.js
    Check a star appears in autocomplete list for a bookmarked page.
    """

    PREF_SUGGEST_SEARCHES = 'browser.urlbar.suggest.searches'

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.bookmark_panel = None
        self.test_urls = [self.marionette.absolute_url('layout/mozilla_grants.html')]

        # Disable search suggestions to only get results for history and bookmarks
        self.prefs.set_pref(self.PREF_SUGGEST_SEARCHES, False)

        with self.marionette.using_context('content'):
            self.marionette.navigate('about:blank')

        self.places.remove_all_history()

    def tearDown(self):
        # Close the autocomplete results
        try:
            if self.bookmark_panel:
                self.marionette.execute_script("""
                  arguments[0].hidePopup();
                """, script_args=[self.bookmark_panel])

            self.browser.navbar.locationbar.autocomplete_results.close()
            self.places.restore_default_bookmarks()
        finally:
            FirefoxTestCase.tearDown(self)

    @skip_under_xvfb
    def test_star_in_autocomplete(self):
        search_string = 'grants'

        def visit_urls():
            with self.marionette.using_context('content'):
                for url in self.test_urls:
                    self.marionette.navigate(url)

        # Navigate to all the urls specified in self.test_urls and wait for them to
        # be registered as visited
        self.places.wait_for_visited(self.test_urls, visit_urls)

        # Bookmark the current page using the bookmark menu
        self.browser.menubar.select_by_id('bookmarksMenu',
                                          'menu_bookmarkThisPage')

        # TODO: Replace hard-coded selector with library method when one is available
        self.bookmark_panel = self.marionette.find_element(By.ID, 'editBookmarkPanel')
        done_button = self.marionette.find_element(By.ID, 'editBookmarkPanelDoneButton')

        Wait(self.marionette).until(
            lambda mn: self.bookmark_panel.get_attribute('panelopen') == 'true')
        done_button.click()

        # We must open the blank page so the autocomplete result isn't "Switch to tab"
        with self.marionette.using_context('content'):
            self.marionette.navigate('about:blank')

        self.places.remove_all_history()

        # Focus the locationbar, delete any contents there, and type the search string
        locationbar = self.browser.navbar.locationbar
        locationbar.clear()
        locationbar.urlbar.send_keys(search_string)
        autocomplete_results = locationbar.autocomplete_results

        # Wait for the search string to be present, for the autocomplete results to appear
        # and for there to be exactly one autocomplete result
        Wait(self.marionette).until(lambda mn: locationbar.value == search_string)
        Wait(self.marionette).until(lambda mn: autocomplete_results.is_complete)
        Wait(self.marionette).until(lambda mn: len(autocomplete_results.visible_results) == 2)

        # Compare the highlighted text in the autocomplete result to the search string
        first_result = autocomplete_results.visible_results[1]
        matching_titles = autocomplete_results.get_matching_text(first_result, 'title')
        for title in matching_titles:
            Wait(self.marionette).until(lambda mn: title.lower() == search_string)

        self.assertIn('bookmark',
                      first_result.get_attribute('type'),
                      'The auto-complete result is a bookmark')
