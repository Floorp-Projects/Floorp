/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
  Ci.nsINavHistoryService
);

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  return cc == 1;
}

// main

add_task(async function test_execute() {
  // we have a new profile, so we should have imported bookmarks
  Assert.equal(histsvc.databaseStatus, histsvc.DATABASE_STATUS_CREATE);

  // add a visit
  var testURI = uri("http://mozilla.com");
  await PlacesTestUtils.addVisits(testURI);

  // now query for the visit, setting sorting and limit such that
  // we should retrieve only the visit we just added
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  for (var i = 0; i < cc; ++i) {
    var node = root.getChild(i);
    // test node properties in RESULTS_AS_VISIT
    Assert.equal(node.uri, testURI.spec);
    Assert.equal(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
  }
  root.containerOpen = false;

  // add another visit for the same URI, and a third visit for a different URI
  var testURI2 = uri("http://google.com/");
  await PlacesTestUtils.addVisits(testURI);
  await PlacesTestUtils.addVisits(testURI2);

  options.maxResults = 5;
  options.resultType = options.RESULTS_AS_URI;

  // test minVisits
  query.minVisits = 0;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.minVisits = 1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.minVisits = 2;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 1);
  query.minVisits = 3;
  result.root.containerOpen = false;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 0);
  result.root.containerOpen = false;

  // test maxVisits
  query.minVisits = -1;
  query.maxVisits = -1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.maxVisits = 0;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 0);
  result.root.containerOpen = false;
  query.maxVisits = 1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 1);
  result.root.containerOpen = false;
  query.maxVisits = 2;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.maxVisits = 3;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 2);
  result.root.containerOpen = false;

  // test annotation-based queries
  await PlacesUtils.history.update({
    url: "http://mozilla.com/",
    annotations: new Map([["testAnno", 123]]),
  });
  query.annotation = "testAnno";
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 1);
  Assert.equal(result.root.getChild(0).uri, "http://mozilla.com/");
  result.root.containerOpen = false;

  // test annotationIsNot
  query.annotationIsNot = true;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.equal(result.root.childCount, 1);
  Assert.equal(result.root.getChild(0).uri, "http://google.com/");
  result.root.containerOpen = false;

  // By default history is enabled.
  Assert.ok(!histsvc.historyDisabled);

  // test getPageTitle
  await PlacesTestUtils.addVisits({
    uri: uri("http://example.com"),
    title: "title",
  });
  let placeInfo = await PlacesUtils.history.fetch("http://example.com");
  Assert.equal(placeInfo.title, "title");

  // query for the visit
  Assert.ok(uri_in_db(testURI));

  // test for schema changes in bug 373239
  // get direct db connection
  var db = histsvc.DBConnection;
  var q = "SELECT id FROM moz_bookmarks";
  var statement;
  try {
    statement = db.createStatement(q);
  } catch (ex) {
    do_throw("bookmarks table does not have id field, schema is too old!");
  } finally {
    statement.finalize();
  }

  // bug 394741 - regressed history text searches
  await PlacesTestUtils.addVisits(uri("http://mozilla.com"));
  options = histsvc.getNewQueryOptions();
  // options.resultType = options.RESULTS_AS_VISIT;
  query = histsvc.getNewQuery();
  query.searchTerms = "moz";
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.ok(root.childCount > 0);
  root.containerOpen = false;
});
