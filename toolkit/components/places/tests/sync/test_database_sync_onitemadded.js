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

 /*
  * This test checks that when we add an item we sync only if its type is
  * TYPE_BOOKMARKS
  */

var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService).
            getBranch("places.");
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);

const TEST_URI = "http://test.com/";

const kSyncPrefName = "syncDBTableIntervalInSecs";
const SYNC_INTERVAL = 600;
const kSyncFinished = "places-sync-finished";

var syncObserver = {
  _numSyncs: 0,
  finalSync: false,
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kSyncFinished) {
      if (++this._numSyncs > 1 || !this.finalSync)
        do_throw("We synced too many times: " + this._numSyncs);

      // remove the observers
      os.removeObserver(this, kSyncFinished);
      bs.removeObserver(bookmarksObserver, false);

      finish_test();
    }
  }
}
os.addObserver(syncObserver, kSyncFinished, false);

var bookmarksObserver = {
  onItemAdded: function(aItemId, aNewParent, aNewIndex) {
    if (bs.getItemType(aItemId) == bs.TYPE_BOOKMARK)
      syncObserver.finalSync = true;
  }
}
bs.addObserver(bookmarksObserver, false);

function run_test()
{
  // dynamic container sample
  do_load_module("/toolkit/components/places/tests/unit/nsDynamicContainerServiceSample.js");

  // First set the preference for the timer to a large value, so it won't sync
  prefs.setIntPref(kSyncPrefName, SYNC_INTERVAL);

  // Add a folder
  bs.createFolder(bs.toolbarFolder, "folder", bs.DEFAULT_INDEX);

  // Add a dynamic container
  bs.createDynamicContainer(bs.toolbarFolder, "dynamic",
                                "@mozilla.org/browser/remote-container-sample;1",
                                bs.DEFAULT_INDEX);

  // Add a separator
  bs.insertSeparator(bs.toolbarFolder, bs.DEFAULT_INDEX);

  // Add a bookmark, this will trigger final sync
  bs.insertBookmark(bs.toolbarFolder, uri(TEST_URI), bs.DEFAULT_INDEX, "bookmark");

  do_test_pending();
}
