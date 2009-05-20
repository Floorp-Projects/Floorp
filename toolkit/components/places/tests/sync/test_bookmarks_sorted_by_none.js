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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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
 * Bug 493933 - When sorting bookmarks BY NONE we should take in count partitioning.
 *
 * When we have a bookmarks results sorted BY NONE, could be the order depends
 * on current status of disk and temp tables due to how the query executes the
 * UNION. We must ensure their order is correct.
 */
 
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

const kSyncPrefName = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 600;
const kSyncFinished = "places-sync-finished";

var observer = {
  _runCount: 0,
  one: null,
  two: null,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished) {
      // Skip the first sync.
      if (++this._runCount < 2)
        return;

      os.removeObserver(this, kSyncFinished);

      // Sanity check positions.
      do_check_true(this.one < this.two);
      do_check_eq(bs.getItemIndex(this.one), 0);
      do_check_eq(bs.getItemIndex(this.two), 1);

      check_results();

      // Now add a visit to the second bookmark, so that it is in temp table.
      hs.addVisit(uri("http://2.mozilla.org/"), Date.now() * 1000, null,
                  hs.TRANSITION_TYPED, false, 0);

      // Nothing should change in results.
      check_results();

      // Cleanup.
      bs.removeFolderChildren(bs.toolbarFolder);
      finish_test();
    }
  }
}
os.addObserver(observer, kSyncFinished, false);

function check_results() {
  // Create a bookmarks query.
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder], 1);
  query.searchTerms = "mozilla";
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 2);
  do_check_eq(root.getChild(0).title, "mozilla 1");
  do_check_eq(root.getChild(1).title, "mozilla 2");
  root.containerOpen = false;
}

function run_test()
{
  // First set the preference for the timer to a large value so we don't sync
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Add two bookmarks and check their index
  observer.one = bs.insertBookmark(bs.toolbarFolder,
                                   uri("http://1.mozilla.org/"),
                                   bs.DEFAULT_INDEX, "mozilla 1");
  observer.two = bs.insertBookmark(bs.toolbarFolder,
                                   uri("http://2.mozilla.org/"),
                                   bs.DEFAULT_INDEX, "mozilla 2");

  do_test_pending();
}
