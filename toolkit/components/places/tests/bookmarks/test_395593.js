/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function check_queries_results(aQueries, aOptions, aExpectedBookmarks) {
  var result = PlacesUtils.history.executeQueries(aQueries, aQueries.length, aOptions);
  var root = result.root;
  root.containerOpen = true;

  // Dump found nodes.
  for (let i = 0; i < root.childCount; i++) {
    dump("nodes[" + i + "]: " + root.getChild(0).title + "\n");
  }

  Assert.equal(root.childCount, aExpectedBookmarks.length);
  for (let i = 0; i < root.childCount; i++) {
    Assert.equal(root.getChild(i).bookmarkGuid, aExpectedBookmarks[i].guid);
  }

  root.containerOpen = false;
}

// main
add_task(async function run_test() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "123 0",
      url: "http://foo.tld",
    }, {
      title: "456",
      url: "http://foo.tld",
    }, {
      title: "123 456",
      url: "http://foo.tld",
    }, {
      title: "789 456",
      url: "http://foo.tld",
    }]
  });

  /**
   * All of the query objects are ORed together. Within a query, all the terms
   * are ANDed together. See nsINavHistory.idl.
   */
  var queries = [];
  queries.push(PlacesUtils.history.getNewQuery());
  queries.push(PlacesUtils.history.getNewQuery());
  var options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  // Test 1
  dump("Test searching for 123 OR 789\n");
  queries[0].searchTerms = "123";
  queries[1].searchTerms = "789";
  check_queries_results(queries, options, [
    bookmarks[0],
    bookmarks[2],
    bookmarks[3]
  ]);

  // Test 2
  dump("Test searching for 123 OR 456\n");
  queries[0].searchTerms = "123";
  queries[1].searchTerms = "456";
  check_queries_results(queries, options, bookmarks);

  // Test 3
  dump("Test searching for 00 OR 789\n");
  queries[0].searchTerms = "00";
  queries[1].searchTerms = "789";
  check_queries_results(queries, options, [bookmarks[3]]);
});
