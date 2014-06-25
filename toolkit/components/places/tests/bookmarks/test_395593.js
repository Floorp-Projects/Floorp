/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

function check_queries_results(aQueries, aOptions, aExpectedItemIds) {
  var result = hs.executeQueries(aQueries, aQueries.length, aOptions);
  var root = result.root;
  root.containerOpen = true;

  // Dump found nodes.
  for (let i = 0; i < root.childCount; i++) {
    dump("nodes[" + i + "]: " + root.getChild(0).title + "\n");
  }

  do_check_eq(root.childCount, aExpectedItemIds.length);
  for (let i = 0; i < root.childCount; i++) {
    do_check_eq(root.getChild(i).itemId, aExpectedItemIds[i]);
  }

  root.containerOpen = false;
}

// main
function run_test() {
  var id1 = bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://foo.tld"),
                              bs.DEFAULT_INDEX, "123 0");
  var id2 = bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://foo.tld"),
                              bs.DEFAULT_INDEX, "456");
  var id3 = bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://foo.tld"),
                              bs.DEFAULT_INDEX, "123 456");
  var id4 = bs.insertBookmark(bs.bookmarksMenuFolder, uri("http://foo.tld"),
                              bs.DEFAULT_INDEX, "789 456");

  /**
   * All of the query objects are ORed together. Within a query, all the terms
   * are ANDed together. See nsINavHistory.idl.
   */
  var queries = [];
  queries.push(hs.getNewQuery());
  queries.push(hs.getNewQuery());
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  // Test 1
  dump("Test searching for 123 OR 789\n");
  queries[0].searchTerms = "123";
  queries[1].searchTerms = "789";
  check_queries_results(queries, options, [id1, id3, id4]);

  // Test 2
  dump("Test searching for 123 OR 456\n");
  queries[0].searchTerms = "123";
  queries[1].searchTerms = "456";
  check_queries_results(queries, options, [id1, id2, id3, id4]);

  // Test 3
  dump("Test searching for 00 OR 789\n");
  queries[0].searchTerms = "00";
  queries[1].searchTerms = "789";
  check_queries_results(queries, options, [id4]);
}
