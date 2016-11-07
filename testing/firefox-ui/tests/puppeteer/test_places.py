# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_ui_harness.testcases import FirefoxTestCase


class TestPlaces(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.urls = [self.marionette.absolute_url('layout/mozilla_governance.html'),
                     self.marionette.absolute_url('layout/mozilla_grants.html'),
                     ]

    def tearDown(self):
        try:
            self.puppeteer.places.restore_default_bookmarks()
            self.puppeteer.places.remove_all_history()
        finally:
            FirefoxTestCase.tearDown(self)

    def get_all_urls_in_history(self):
        return self.marionette.execute_script("""
          let hs = Components.classes["@mozilla.org/browser/nav-history-service;1"]
                   .getService(Components.interfaces.nsINavHistoryService);
          let urls = [];

          let options = hs.getNewQueryOptions();
          options.resultType = options.RESULTS_AS_URI;

          let root = hs.executeQuery(hs.getNewQuery(), options).root
          root.containerOpen = true;
          for (let i = 0; i < root.childCount; i++) {
            urls.push(root.getChild(i).uri)
          }
          root.containerOpen = false;

          return urls;
        """)

    def test_plugins(self):
        # TODO: Once we use a plugin, add a test case to verify that the data will be removed
        self.puppeteer.places.clear_plugin_data()

    def test_bookmarks(self):
        star_button = self.marionette.find_element(By.ID, 'bookmarks-menu-button')

        # Visit URLs and bookmark them all
        for url in self.urls:
            with self.marionette.using_context('content'):
                self.marionette.navigate(url)

            Wait(self.marionette).until(
                lambda _: self.puppeteer.places.is_bookmark_star_button_ready())
            star_button.click()
            Wait(self.marionette).until(lambda _: self.puppeteer.places.is_bookmarked(url))

            ids = self.puppeteer.places.get_folder_ids_for_url(url)
            self.assertEqual(len(ids), 1)
            self.assertEqual(ids[0], self.puppeteer.places.bookmark_folders.unfiled)

        # Restore default bookmarks, so the added URLs are gone
        self.puppeteer.places.restore_default_bookmarks()
        for url in self.urls:
            self.assertFalse(self.puppeteer.places.is_bookmarked(url))

    def test_history(self):
        self.assertEqual(len(self.get_all_urls_in_history()), 0)

        # Visit pages and check that they are all present
        def load_urls():
            with self.marionette.using_context('content'):
                for url in self.urls:
                    self.marionette.navigate(url)
        self.puppeteer.places.wait_for_visited(self.urls, load_urls)

        self.assertEqual(self.get_all_urls_in_history(), self.urls)

        # Check that both pages are no longer in the remove_all_history
        self.puppeteer.places.remove_all_history()
        self.assertEqual(len(self.get_all_urls_in_history()), 0)
