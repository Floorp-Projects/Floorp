/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Geo Mealer <gmealer@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * @fileoverview
 * The ModalDialogAPI adds support for handling modal dialogs. It
 * has to be used e.g. for alert boxes and other commonDialog instances.
 *
 * @version 1.0.2
 */

var MODULE_NAME = 'PlacesAPI';

const gTimeout = 5000;

/**
 * Instance of the bookmark service to gain access to the bookmark API.
 *
 * @see http://mxr.mozilla.org/mozilla-central (nsINavBookmarksService.idl)
 */
var bookmarksService = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                       getService(Ci.nsINavBookmarksService);

/**
 * Instance of the history service to gain access to the history API.
 *
 * @see http://mxr.mozilla.org/mozilla-central (nsINavHistoryService.idl)
 */
var historyService = Cc["@mozilla.org/browser/nav-history-service;1"].
                     getService(Ci.nsINavHistoryService);

/**
 * Instance of the livemark service to gain access to the livemark API
 *
 * @see http://mxr.mozilla.org/mozilla-central (nsILivemarkService.idl)
 */
var livemarkService = Cc["@mozilla.org/browser/livemark-service;2"].
                      getService(Ci.nsILivemarkService);

/**
 * Instance of the browser history interface to gain access to
 * browser-specific history API
 *
 * @see http://mxr.mozilla.org/mozilla-central (nsIBrowserHistory.idl)
 */
var browserHistory = Cc["@mozilla.org/browser/nav-history-service;1"].
                     getService(Ci.nsIBrowserHistory);

/**
 * Check if an URI is bookmarked within the specified folder
 *
 * @param (nsIURI) uri
 *        URI of the bookmark
 * @param {String} folderId
 *        Folder in which the search has to take place
 * @return Returns if the URI exists in the given folder
 * @type Boolean
 */
function isBookmarkInFolder(uri, folderId)
{
  var ids = bookmarksService.getBookmarkIdsForURI(uri, {});
  for (let i = 0; i < ids.length; i++) {
    if (bookmarksService.getFolderIdForItem(ids[i]) == folderId)
      return true;
  }

  return false;
}

/**
 * Restore the default bookmarks for the current profile
 */
function restoreDefaultBookmarks() {
  // Get the default bookmarks.html
  let dirService = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);

  bookmarksFile = dirService.get("profDef", Ci.nsILocalFile);
  bookmarksFile.append("bookmarks.html");

  // Run the import
  let importer = Cc["@mozilla.org/browser/places/import-export-service;1"].
                 getService(Ci.nsIPlacesImportExportService);
  importer.importHTMLFromFile(bookmarksFile, true);
}

/**
 * Synchronous wrapper around browserHistory.removeAllPages()
 * Removes history and blocks until done
 */
function removeAllHistory() {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";

  // Create flag visible to both the eval and the observer object
  var finishedFlag = {
    state: false
  }

  // Set up an observer so we get notified when remove completes
  var observerService = Cc["@mozilla.org/observer-service;1"].
                        getService(Ci.nsIObserverService);
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      observerService.removeObserver(this, TOPIC_EXPIRATION_FINISHED);    
      finishedFlag.state = true;
    }
  }
  observerService.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  // Remove the pages, then block until we're done or until timeout is reached
  browserHistory.removeAllPages();
  mozmill.controller.waitForEval("subject.state == true", gTimeout, 100, finishedFlag);
}
