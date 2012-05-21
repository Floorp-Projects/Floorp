/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  // test search on folder with various sorts and max results
  // see bug #385829 for more details
  var folder = bmsvc.createFolder(root, "bug 385829 test", bmsvc.DEFAULT_INDEX);
  var b1 = bmsvc.insertBookmark(folder, uri("http://a1.com/"),
                                bmsvc.DEFAULT_INDEX, "1 title");

  var b2 = bmsvc.insertBookmark(folder, uri("http://a2.com/"),
                                bmsvc.DEFAULT_INDEX, "2 title");

  var b3 = bmsvc.insertBookmark(folder, uri("http://a3.com/"),
                                bmsvc.DEFAULT_INDEX, "3 title");

  var b4 = bmsvc.insertBookmark(folder, uri("http://a4.com/"),
                                bmsvc.DEFAULT_INDEX, "4 title");

  // ensure some unique values for date added and last modified
  // for date added:    b1 < b2 < b3 < b4
  // for last modified: b1 > b2 > b3 > b4
  bmsvc.setItemDateAdded(b1, 1000);
  bmsvc.setItemDateAdded(b2, 2000);
  bmsvc.setItemDateAdded(b3, 3000);
  bmsvc.setItemDateAdded(b4, 4000);

  bmsvc.setItemLastModified(b1, 4000);
  bmsvc.setItemLastModified(b2, 3000);
  bmsvc.setItemLastModified(b3, 2000);
  bmsvc.setItemLastModified(b4, 1000);

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 3;
  query.setFolders([folder], 1);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // test SORT_BY_DATEADDED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_DATEADDED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b3);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b1);
  do_check_true(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_LASTMODIFIED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b3);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b1);
  do_check_true(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);

  // test SORT_BY_LASTMODIFIED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;

  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_ASCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_DESCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b4);
  do_check_eq(rootNode.getChild(1).itemId, b3);
  do_check_eq(rootNode.getChild(2).itemId, b2);
  do_check_true(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  do_check_true(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_ASCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b4);
  do_check_eq(rootNode.getChild(1).itemId, b3);
  do_check_eq(rootNode.getChild(2).itemId, b2);
  do_check_true(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_DESCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;
  result = histsvc.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  do_check_eq(rootNode.getChild(1).itemId, b2);
  do_check_eq(rootNode.getChild(2).itemId, b3);
  do_check_true(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  do_check_true(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;
}
