/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *   Marco Bonardo <mak77@bonardo.net>
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

var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");

const TEST_URI = "http://test.com/";
const MODIFIED_URI = "http://test.com/index.html";

const SYNC_INTERVAL = 600; // ten minutes
const kSyncPrefName = "syncDBTableIntervalInSecs";
const kSyncFinished = "places-sync-finished";

// Used to update observer itemId
var bookmarksObserver = {
  onItemAdded: function(aItemId, aNewParent, aNewIndex) {
    observer.itemId = aItemId;
  },
  onItemChanged: function(aItemId, aProperty, aValue) {
    if (aProperty == "uri")
      do_check_eq(observer.itemId, aItemId);
  }
}
bs.addObserver(bookmarksObserver, false);

var observer = {
  itemId: -1,
  _runCount: 0,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished) {
      // item id must be valid
      do_check_neq(this.itemId, -1);
      if (++this._runCount == 1) {
        // First sync is fired by adding the bookmark
        // Check that tables have been synced after insertBookmark
        new_test_bookmark_uri_event(this.itemId, TEST_URI, true);
        // Now modify the bookmark
        bs.changeBookmarkURI(this.itemId, uri(MODIFIED_URI));
      }
      else if (this._runCount == 2) {
        // Second sync is fired by changing the bookmark's uri
        // remove the observer, we don't need to observe sync on quit
        os.removeObserver(this, kSyncFinished);
        bs.removeObserver(bookmarksObserver);
        // Check that tables have been synced after changeBookmarkURI
        new_test_bookmark_uri_event(this.itemId, MODIFIED_URI, true, true);
      }
      else
        do_throw("Too many places sync calls");
    }
  }
}
os.addObserver(observer, kSyncFinished, false);

function run_test()
{
  // Set the preference for the timer to a really large value, so it won't
  // run before the test finishes.
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Insert a new bookmark
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri(TEST_URI),
                    bs.DEFAULT_INDEX, "test");

  do_test_pending();
}
