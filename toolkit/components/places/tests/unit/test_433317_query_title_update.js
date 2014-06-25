/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  } catch(ex) {
    do_throw("Unable to initialize Places services");
  }

  // create a query bookmark
  var queryId = bmsvc.insertBookmark(bmsvc.toolbarFolder, uri("place:"),
                                     0 /* first item */, "test query");

  // query for that query
  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.toolbarFolder], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var queryNode = root.getChild(0);
  do_check_eq(queryNode.title, "test query");

  // change the title
  bmsvc.setItemTitle(queryId, "foo");

  // confirm the node was updated
  do_check_eq(queryNode.title, "foo");

  root.containerOpen = false;
}
