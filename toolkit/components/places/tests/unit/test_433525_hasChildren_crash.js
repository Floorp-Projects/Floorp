/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  try {
    var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);
    var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  } catch(ex) {
    do_throw("Unable to initialize Places services");
  }

  // add a visit
  var testURI = uri("http://test");
  yield PlacesTestUtils.addVisits(testURI);

  // query for the visit
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  query.uri = testURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;

  // check hasChildren while the container is closed
  do_check_eq(root.hasChildren, true);

  // now check via the saved search path
  var queryURI = histsvc.queriesToQueryString([query], 1, options);
  bmsvc.insertBookmark(bmsvc.toolbarFolder, uri(queryURI),
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
  queryNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(queryNode.hasChildren, true);
  root.containerOpen = false;
});
