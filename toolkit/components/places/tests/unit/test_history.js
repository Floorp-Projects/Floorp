/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Dan Mills <thunder@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Get history services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
var gh = histsvc.QueryInterface(Ci.nsIGlobalHistory2);
var bh = histsvc.QueryInterface(Ci.nsIBrowserHistory);

/**
 * Adds a test URI visit to the database, and checks for a valid place ID.
 *
 * @param aURI
 *        The URI to add a visit for.
 * @param aReferrer
 *        The referring URI for the given URI.  This can be null.
 * @returns the place id for aURI.
 */
function add_visit(aURI, aReferrer) {
  var visitId = histsvc.addVisit(aURI,
                                 Date.now() * 1000,
                                 aReferrer,
                                 histsvc.TRANSITION_TYPED, // user typed in URL bar
                                 false, // not redirect
                                 0);
  dump("### Added visit with id of " + visitId + "\n");
  do_check_true(gh.isVisited(aURI));
  do_check_guid_for_uri(aURI);
  return visitId;
}

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
function run_test() {
  // we have a new profile, so we should have imported bookmarks
  do_check_eq(histsvc.databaseStatus, histsvc.DATABASE_STATUS_CREATE);

  // add a visit
  var testURI = uri("http://mozilla.com");
  add_visit(testURI);

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
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
    // TODO: change query type to RESULTS_AS_FULL_VISIT and test this
    //do_check_eq(node.transitionType, histsvc.TRANSITION_TYPED);
  }
  root.containerOpen = false;

  // add another visit for the same URI, and a third visit for a different URI
  var testURI2 = uri("http://google.com/");
  add_visit(testURI);
  add_visit(testURI2);

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
  var title = histsvc.getPageTitle(uri("http://mozilla.com"));
  do_check_eq(title, null);

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
  add_visit(uri("http://mozilla.com"));
  var options = histsvc.getNewQueryOptions();
  //options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  query.searchTerms = "moz";
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_true(root.childCount > 0);
  root.containerOpen = false;

  // bug 400544 - testing that a referrer that is not in the DB gets added
  var referrerURI = uri("http://yahoo.com");
  do_check_false(uri_in_db(referrerURI));
  add_visit(uri("http://mozilla.com"), referrerURI);
  do_check_true(uri_in_db(referrerURI));
}
