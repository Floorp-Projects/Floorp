# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import namedtuple
from time import sleep

from marionette_driver.errors import MarionetteException, TimeoutException

from firefox_puppeteer.base import BaseLib


class Places(BaseLib):
    """Low-level access to several bookmark and history related actions."""

    BookmarkFolders = namedtuple('bookmark_folders',
                                 ['root', 'menu', 'toolbar', 'tags', 'unfiled'])
    bookmark_folders = BookmarkFolders(1, 2, 3, 4, 5)

    # Bookmark related helpers #

    def is_bookmarked(self, url):
        """Checks if the given URL is bookmarked.

        :param url: The URL to Check

        :returns: True, if the URL is a bookmark
        """
        return self.marionette.execute_script("""
          let url = arguments[0];

          let bs = Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"]
                   .getService(Components.interfaces.nsINavBookmarksService);
          let ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

          let uri = ios.newURI(url, null, null);
          let results = bs.getBookmarkIdsForURI(uri, {});

          return results.length == 1;
        """, script_args=[url])

    def get_folder_ids_for_url(self, url):
        """Retrieves the folder ids where the given URL has been bookmarked in.

         :param url: URL of the bookmark

         :returns: List of folder ids
        """
        return self.marionette.execute_script("""
          let url = arguments[0];

          let bs = Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"]
                   .getService(Components.interfaces.nsINavBookmarksService);
          let ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

          let bookmarkIds = bs.getBookmarkIdsForURI(ios.newURI(url, null, null), {});
          let folderIds = [];

          for (let i = 0; i < bookmarkIds.length; i++) {
            folderIds.push(bs.getFolderIdForItem(bookmarkIds[i]));
          }

          return folderIds;
        """, script_args=[url])

    def is_bookmark_star_button_ready(self):
        """Checks if the status of the star-button is not updating.

        :returns: True, if the button is ready
        """
        return self.marionette.execute_script("""
          let button = window.BookmarkingUI;

          return button.status !== button.STATUS_UPDATING;
        """)

    def restore_default_bookmarks(self):
        """Restores the default bookmarks for the current profile."""
        retval = self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/BookmarkHTMLUtils.jsm");
          Components.utils.import("resource://gre/modules/Services.jsm");

          // Default bookmarks.html file is stored inside omni.jar,
          // so get it via a resource URI
          let defaultBookmarks = 'chrome://browser/locale/bookmarks.html';

          let observer = {
            observe: function (aSubject, aTopic, aData) {
              Services.obs.removeObserver(observer, "bookmarks-restore-success");
              Services.obs.removeObserver(observer, "bookmarks-restore-failed");

              marionetteScriptFinished(aTopic == "bookmarks-restore-success");
            }
          };

          // Trigger the import of the default bookmarks
          Services.obs.addObserver(observer, "bookmarks-restore-success", false);
          Services.obs.addObserver(observer, "bookmarks-restore-failed", false);
          BookmarkHTMLUtils.importFromURL(defaultBookmarks, true);
        """, script_timeout=10000)

        if not retval:
            raise MarionetteException("Restore Default Bookmarks failed")

    # Browser history related helpers #

    def get_all_urls_in_history(self):
        return self.marionette.execute_script("""
          let hs = Cc["@mozilla.org/browser/nav-history-service;1"]
                   .getService(Ci.nsINavHistoryService);
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

    def remove_all_history(self):
        """Removes all history items."""
        with self.marionette.using_context('chrome'):
            try:
                self.marionette.execute_async_script("""
                    Components.utils.import("resource://gre/modules/Services.jsm");

                    let hs = Components.classes["@mozilla.org/browser/nav-history-service;1"]
                             .getService(Components.interfaces.nsIBrowserHistory);

                    let observer = {
                      observe: function (aSubject, aTopic, aData) {
                        Services.obs.removeObserver(observer, 'places-expiration-finished');

                        marionetteScriptFinished(true);
                      }
                    };

                    // Remove the pages, then block until we're done or until timeout is reached
                    Services.obs.addObserver(observer, 'places-expiration-finished', false);

                    hs.removeAllPages();
                """, script_timeout=10000)
            except TimeoutException:
                # TODO: In case of a timeout clean-up the registered topic
                pass

    def wait_for_visited(self, urls, callback):
        """Waits until all passed-in urls have been visited.

        :param urls: List of URLs which need to be visited and indexed

        :param callback: Method to execute which triggers loading of the URLs
        """
        # Bug 1121691: Needs observer handling support with callback first
        # Until then we have to wait about 4s to ensure the page has been indexed
        callback()
        sleep(4)

    # Plugin related helpers #

    def clear_plugin_data(self):
        """Clears any kind of locally stored data from plugins."""
        self.marionette.execute_script("""
          let host = Components.classes["@mozilla.org/plugin/host;1"]
                     .getService(Components.interfaces.nsIPluginHost);
          let tags = host.getPluginTags();

          tags.forEach(aTag => {
            try {
              host.clearSiteData(aTag, null, Components.interfaces.nsIPluginHost
                  .FLAG_CLEAR_ALL, -1);
            } catch (ex) {
            }
          });
        """)
