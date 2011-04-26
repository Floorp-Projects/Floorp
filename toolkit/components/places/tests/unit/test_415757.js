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
 *  Marco Bonardo <mak77@supereva.it>
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
    do_throw("addPageWithDetails failed");
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
