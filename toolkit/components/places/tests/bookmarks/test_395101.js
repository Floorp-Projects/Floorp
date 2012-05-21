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

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}

// get bookmarks root id
var root = bmsvc.bookmarksMenuFolder;

// main
function run_test() {
  // test searching for tagged bookmarks

  // test folder
  var folder = bmsvc.createFolder(root, "bug 395101 test", bmsvc.DEFAULT_INDEX);

  // create a bookmark
  var testURI = uri("http://a1.com");
  var b1 = bmsvc.insertBookmark(folder, testURI,
                                bmsvc.DEFAULT_INDEX, "1 title");

  // tag the bookmarked URI
  tagssvc.tagURI(testURI, ["elephant", "walrus", "giraffe", "turkey", "hiPPo", "BABOON", "alf"]);

  // search for the bookmark, using a tag
  var query = histsvc.getNewQuery();
  query.searchTerms = "elephant";
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  query.setFolders([folder], 1);

  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;

  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  rootNode.containerOpen = false;

  // partial matches are okay
  query.searchTerms = "wal";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  rootNode.containerOpen = false;

  // case insensitive search term
  query.searchTerms = "WALRUS";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  rootNode.containerOpen = false;

  // case insensitive tag
  query.searchTerms = "baboon";
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  do_check_eq(rootNode.getChild(0).itemId, b1);
  rootNode.containerOpen = false;
}
