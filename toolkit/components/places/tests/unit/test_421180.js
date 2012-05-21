/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
