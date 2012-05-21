/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get global history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"]
                .getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"]
                  .getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
}

// Get annotation service
try {
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"]
                  .getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
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

const TOTAL_SITES = 20;

// main
function run_test() {

  // add pages to global history
  try {
    for (var i = 0; i < TOTAL_SITES; i++) {
      let site = "http://www.test-" + i + ".com/";
      let testURI = uri(site);
      let when = Date.now() * 1000 + (i * TOTAL_SITES);
      histsvc.addVisit(testURI, when, null, histsvc.TRANSITION_TYPED, false, 0);
    }
    for (var i = 0; i < TOTAL_SITES; i++) {
      let site = "http://www.test.com/" + i + "/";
      let testURI = uri(site);
      let when = Date.now() * 1000 + (i * TOTAL_SITES);
      histsvc.addVisit(testURI, when, null, histsvc.TRANSITION_TYPED, false, 0);
    }
  } catch(ex) {
    do_throw("addVisit failed");
  }

  // set a page annotation on one of the urls that will be removed
  var testAnnoDeletedURI = uri("http://www.test.com/1/");
  var testAnnoDeletedName = "foo";
  var testAnnoDeletedValue = "bar";
  try {
    annosvc.setPageAnnotation(testAnnoDeletedURI, testAnnoDeletedName,
                              testAnnoDeletedValue, 0,
                              annosvc.EXPIRE_WITH_HISTORY);
  } catch(ex) {
    do_throw("setPageAnnotation failed");
  }

  // set a page annotation on one of the urls that will NOT be removed
  var testAnnoRetainedURI = uri("http://www.test-1.com/");
  var testAnnoRetainedName = "foo";
  var testAnnoRetainedValue = "bar";
  try {
    annosvc.setPageAnnotation(testAnnoRetainedURI, testAnnoRetainedName,
                              testAnnoRetainedValue, 0,
                              annosvc.EXPIRE_WITH_HISTORY);
  } catch(ex) {
    do_throw("setPageAnnotation failed");
  }

  // remove pages from www.test.com
  bhist.removePagesFromHost("www.test.com", false);

  // check that all pages in www.test.com have been removed
  for (var i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test.com/" + i + "/";
    let testURI = uri(site);
    do_check_false(uri_in_db(testURI));
  }

  // check that all pages in www.test-X.com have NOT been removed
  for (var i = 0; i < TOTAL_SITES; i++) {
    let site = "http://www.test-" + i + ".com/";
    let testURI = uri(site);
    do_check_true(uri_in_db(testURI));
  }

  // check that annotation on the removed item does not exists
  try {
    annosvc.getPageAnnotation(testAnnoDeletedURI, testAnnoName);
    do_throw("fetching page-annotation that doesn't exist, should've thrown");
  } catch(ex) {}

  // check that annotation on the NOT removed item still exists
  try {
    var annoVal = annosvc.getPageAnnotation(testAnnoRetainedURI,
                                            testAnnoRetainedName);
  } catch(ex) {
    do_throw("The annotation has been removed erroneously");
  }
  do_check_eq(annoVal, testAnnoRetainedValue);

}
