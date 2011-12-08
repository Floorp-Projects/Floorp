/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@supereva.it> (Original Author)
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

// Get bookmarks service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
}
catch(ex) {
  do_throw("Could not get bookmarks service\n");
}

// Get database connection
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var mDBConn = histsvc.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
}
catch(ex) {
  do_throw("Could not get database connection\n");
}

add_test(function test_keywordRemovedOnUniqueItemRemoval() {
  var bookmarkedURI = uri("http://foo.bar");
  var keyword = "testkeyword";

  // TEST 1
  // 1. add a bookmark
  // 2. add a keyword to it
  // 3. remove bookmark
  // 4. check that keyword has gone
  var bookmarkId = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,
                                        bookmarkedURI,
                                        bmsvc.DEFAULT_INDEX,
                                        "A bookmark");
  bmsvc.setKeywordForBookmark(bookmarkId, keyword);
  // remove bookmark
  bmsvc.removeItem(bookmarkId);

  waitForAsyncUpdates(function() {
    // Check that keyword has been removed from the database.
    // The removal is asynchronous.
    var sql = "SELECT id FROM moz_keywords WHERE keyword = ?1";
    var stmt = mDBConn.createStatement(sql);
    stmt.bindByIndex(0, keyword);
    do_check_false(stmt.executeStep());
    stmt.finalize();

    run_next_test();
  });
});

add_test(function test_keywordNotRemovedOnNonUniqueItemRemoval() {
  var bookmarkedURI = uri("http://foo.bar");
  var keyword = "testkeyword";

  // TEST 2
  // 1. add 2 bookmarks
  // 2. add the same keyword to them
  // 3. remove first bookmark
  // 4. check that keyword is still there
  var bookmarkId1 = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,
                                        bookmarkedURI,
                                        bmsvc.DEFAULT_INDEX,
                                        "A bookmark");
  bmsvc.setKeywordForBookmark(bookmarkId1, keyword);

  var bookmarkId2 = bmsvc.insertBookmark(bmsvc.toolbarFolder,
                                        bookmarkedURI,
                                        bmsvc.DEFAULT_INDEX,
                                        keyword);
  bmsvc.setKeywordForBookmark(bookmarkId2, keyword);

  // remove first bookmark
  bmsvc.removeItem(bookmarkId1);

  waitForAsyncUpdates(function() {
    // check that keyword is still there
    var sql = "SELECT id FROM moz_keywords WHERE keyword = ?1";
    var stmt = mDBConn.createStatement(sql);
    stmt.bindByIndex(0, keyword);
    do_check_true(stmt.executeStep());
    stmt.finalize();

    run_next_test();
  });
});

function run_test() {
  run_next_test();
}
