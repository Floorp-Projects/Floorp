/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
} catch (ex) {
  do_throw("Could not get history services\n");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch (ex) {
  do_throw("Could not get the nav-bookmarks-service\n");
}

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch (ex) {
  do_throw("Could not get tagging service\n");
}


// main
function run_test() {
  var uri1 = uri("http://foo.bar/");

  // create 2 bookmarks on the same uri
  bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, uri1,
                       bmsvc.DEFAULT_INDEX, "title 1");
  bmsvc.insertBookmark(bmsvc.toolbarFolder, uri1,
                       bmsvc.DEFAULT_INDEX, "title 2");
  // add some tags
  tagssvc.tagURI(uri1, ["foo", "bar", "foobar", "foo bar"]);

  // check that a generic bookmark query returns only real bookmarks
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;

  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, 2);
  var node1 = root.getChild(0);
  do_check_eq(bmsvc.getFolderIdForItem(node1.itemId), bmsvc.bookmarksMenuFolder);
  var node2 = root.getChild(1);
  do_check_eq(bmsvc.getFolderIdForItem(node2.itemId), bmsvc.toolbarFolder);
  root.containerOpen = false;
}
