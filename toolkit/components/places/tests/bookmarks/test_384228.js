/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// get bookmarks root id
var root = bmsvc.bookmarksMenuFolder;

// main
function run_test() {
  // test querying for bookmarks in multiple folders
  var testFolder1 = bmsvc.createFolder(root, "bug 384228 test folder 1",
                                       bmsvc.DEFAULT_INDEX);
  do_check_eq(bmsvc.getItemIndex(testFolder1), 0);
  var testFolder2 = bmsvc.createFolder(root, "bug 384228 test folder 2",
                                       bmsvc.DEFAULT_INDEX);
  do_check_eq(bmsvc.getItemIndex(testFolder2), 1);
  var testFolder3 = bmsvc.createFolder(root, "bug 384228 test folder 3",
                                       bmsvc.DEFAULT_INDEX);
  do_check_eq(bmsvc.getItemIndex(testFolder3), 2);

  var b1 = bmsvc.insertBookmark(testFolder1, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b1 (folder 1)");
  do_check_eq(bmsvc.getItemIndex(b1), 0);
  var b2 = bmsvc.insertBookmark(testFolder1, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b2 (folder 1)");
  do_check_eq(bmsvc.getItemIndex(b2), 1);
  var b3 = bmsvc.insertBookmark(testFolder2, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b3 (folder 2)");
  do_check_eq(bmsvc.getItemIndex(b3), 0);
  var b4 = bmsvc.insertBookmark(testFolder3, uri("http://foo.tld/"),
                                bmsvc.DEFAULT_INDEX, "title b4 (folder 3)");
  do_check_eq(bmsvc.getItemIndex(b4), 0);
  // also test recursive search
  var testFolder1_1 = bmsvc.createFolder(testFolder1, "bug 384228 test folder 1.1",
                                         bmsvc.DEFAULT_INDEX);
  do_check_eq(bmsvc.getItemIndex(testFolder1_1), 2);
  var b5 = bmsvc.insertBookmark(testFolder1_1, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "title b5 (folder 1.1)");
  do_check_eq(bmsvc.getItemIndex(b5), 0);

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.searchTerms = "title";
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  query.setFolders([testFolder1, testFolder2], 2);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // should not match item from folder 3
  do_check_eq(rootNode.childCount, 4);

  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_eq(rootNode.getChild(3).itemId, b5);

  rootNode.containerOpen = false;
}
