/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);

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
  options.resultType = options.RESULTS_AS_URI
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  return (cc == 1);
}

// main
function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  // we have a new profile, so we should have imported bookmarks
  do_check_eq(histsvc.databaseStatus, histsvc.DATABASE_STATUS_CREATE);

  // add a visit
  var testURI = uri("http://mozilla.com");
  yield promiseAddVisits(testURI);

  // now query for the visit, setting sorting and limit such that
  // we should retrieve only the visit we just added
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.maxResults = 1;
  // TODO: using full visit crashes in xpcshell test
  //options.resultType = options.RESULTS_AS_FULL_VISIT;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  for (var i=0; i < cc; ++i) {
    var node = root.getChild(i);
    // test node properties in RESULTS_AS_VISIT
    do_check_eq(node.uri, testURI.spec);
    do_check_eq(node.type, Ci.nsINavHistoryResultNode.RESULT_TYPE_URI);
    // TODO: change query type to RESULTS_AS_FULL_VISIT and test this
    //do_check_eq(node.transitionType, histsvc.TRANSITION_TYPED);
  }
  root.containerOpen = false;

  // add another visit for the same URI, and a third visit for a different URI
  var testURI2 = uri("http://google.com/");
  yield promiseAddVisits(testURI);
  yield promiseAddVisits(testURI2);

  options.maxResults = 5;
  options.resultType = options.RESULTS_AS_URI;

  // test minVisits
  query.minVisits = 0;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.minVisits = 1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.minVisits = 2;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 1);
  query.minVisits = 3;
  result.root.containerOpen = false;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 0);
  result.root.containerOpen = false;

  // test maxVisits
  query.minVisits = -1;
  query.maxVisits = -1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.maxVisits = 0;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 0);
  result.root.containerOpen = false;
  query.maxVisits = 1;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 1);
  result.root.containerOpen = false;
  query.maxVisits = 2;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 2);
  result.root.containerOpen = false;
  query.maxVisits = 3;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 2);
  result.root.containerOpen = false;
  
  // test annotation-based queries
  var annos = Cc["@mozilla.org/browser/annotation-service;1"].
              getService(Ci.nsIAnnotationService);
  annos.setPageAnnotation(uri("http://mozilla.com/"), "testAnno", 0, 0,
                          Ci.nsIAnnotationService.EXPIRE_NEVER);
  query.annotation = "testAnno";
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 1);
  do_check_eq(result.root.getChild(0).uri, "http://mozilla.com/");
  result.root.containerOpen = false;

  // test annotationIsNot
  query.annotationIsNot = true;
  result = histsvc.executeQuery(query, options);
  result.root.containerOpen = true;
  do_check_eq(result.root.childCount, 1);
  do_check_eq(result.root.getChild(0).uri, "http://google.com/");
  result.root.containerOpen = false;

  // By default history is enabled.
  do_check_true(!histsvc.historyDisabled);

  // test getPageTitle
  yield promiseAddVisits({ uri: uri("http://example.com"), title: "title" });
  let placeInfo = yield PlacesUtils.promisePlaceInfo(uri("http://example.com"));
  do_check_eq(placeInfo.title, "title");

  // query for the visit
  do_check_true(uri_in_db(testURI));

  // test for schema changes in bug 373239
  // get direct db connection
  var db = histsvc.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
  var q = "SELECT id FROM moz_bookmarks";
  var statement;
  try {
     statement = db.createStatement(q);
  } catch(ex) {
    do_throw("bookmarks table does not have id field, schema is too old!");
  }
  finally {
    statement.finalize();
  }

  // bug 394741 - regressed history text searches
  yield promiseAddVisits(uri("http://mozilla.com"));
  var options = histsvc.getNewQueryOptions();
  //options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  query.searchTerms = "moz";
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_true(root.childCount > 0);
  root.containerOpen = false;
});
