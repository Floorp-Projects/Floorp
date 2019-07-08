/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_execute() {
  try {
    var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
      Ci.nsINavHistoryService
    );
  } catch (ex) {
    do_throw("Unable to initialize Places services");
  }

  // add a visit
  var testURI = uri("http://test");
  await PlacesTestUtils.addVisits(testURI);

  // query for the visit
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  query.uri = testURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;

  // check hasChildren while the container is closed
  Assert.equal(root.hasChildren, true);

  // now check via the saved search path
  var queryURI = histsvc.queryToQueryString(query, options);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "test query",
    url: queryURI,
  });

  // query for that query
  options = histsvc.getNewQueryOptions();
  query = histsvc.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  var queryNode = root.getChild(0);
  Assert.equal(queryNode.title, "test query");
  queryNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  Assert.equal(queryNode.hasChildren, true);
  root.containerOpen = false;
});
