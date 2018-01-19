# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from collections import namedtuple
from time import sleep

from marionette_driver.errors import MarionetteException

from firefox_puppeteer.base import BaseLib


class Places(BaseLib):
    """Low-level access to several bookmark and history related actions."""

    BookmarkFolders = namedtuple('bookmark_folders',
                                 ['root', 'menu', 'toolbar', 'unfiled'])
    bookmark_folders = BookmarkFolders('root________', 'menu________',
                                       'toolbar_____', 'unfiled_____')

    # Bookmark related helpers #

    def is_bookmarked(self, url):
        """Check if the given URL is bookmarked.

        :param url: The URL to Check

        :returns: True, if the URL is a bookmark
        """
        return self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/PlacesUtils.jsm");

          PlacesUtils.bookmarks.fetch({url: arguments[0]}).then(bm => {
            marionetteScriptFinished(bm != null);
          });
        """, script_args=[url])

    def get_folder_ids_for_url(self, url):
        """Retrieve the folder ids where the given URL has been bookmarked in.

        :param url: URL of the bookmark

        :returns: List of folder ids
        """
        return self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/PlacesUtils.jsm");

          let folderGuids = []

          function onResult(bm) {
            folderGuids.push(bm.parentGuid);
          }

          PlacesUtils.bookmarks.fetch({url: arguments[0]}, onResult).then(() => {
            marionetteScriptFinished(folderGuids);
          });
        """, script_args=[url])

    def is_bookmark_star_button_ready(self):
        """Check if the status of the star-button is not updating.

        :returns: True, if the button is ready
        """
        return self.marionette.execute_script("""
          let button = window.BookmarkingUI;

          return button.status !== button.STATUS_UPDATING;
        """)

    def restore_default_bookmarks(self):
        """Restore the default bookmarks for the current profile."""
        retval = self.marionette.execute_async_script("""
          Components.utils.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

          // Default bookmarks.html file is stored inside omni.jar,
          // so get it via a resource URI
          let defaultBookmarks = 'chrome://browser/locale/bookmarks.html';

          // Trigger the import of the default bookmarks
          BookmarkHTMLUtils.importFromURL(defaultBookmarks, true)
                           .then(() => marionetteScriptFinished(true))
                           .catch(() => marionetteScriptFinished(false));
        """, script_timeout=10000)

        if not retval:
            raise MarionetteException("Restore Default Bookmarks failed")

    # Browser history related helpers #

    def get_all_urls_in_history(self):
        """Retrieve any URLs which have been stored in the history."""
        return self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/PlacesUtils.jsm");

          let options = PlacesUtils.history.getNewQueryOptions();
          let root = PlacesUtils.history.executeQuery(
                       PlacesUtils.history.getNewQuery(), options).root;
          let urls = [];

          root.containerOpen = true;
          for (let i = 0; i < root.childCount; i++) {
            urls.push(root.getChild(i).uri)
          }
          root.containerOpen = false;

          return urls;
        """)

    def remove_all_history(self):
        """Remove all history items."""
        retval = self.marionette.execute_async_script("""
            Components.utils.import("resource://gre/modules/PlacesUtils.jsm");

            PlacesUtils.history.clear()
                       .then(() => marionetteScriptFinished(true))
                       .catch(() => marionetteScriptFinished(false));
        """, script_timeout=10000)

        if not retval:
            raise MarionetteException("Removing all history failed")

    def wait_for_visited(self, urls, callback):
        """Wait until all passed-in urls have been visited.

        :param urls: List of URLs which need to be visited and indexed

        :param callback: Method to execute which triggers loading of the URLs
        """
        # Bug 1121691: Needs observer handling support with callback first
        # Until then we have to wait about 4s to ensure the page has been indexed
        callback()
        sleep(4)

    # Plugin related helpers #

    def clear_plugin_data(self):
        """Clear any kind of locally stored data from plugins."""
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
