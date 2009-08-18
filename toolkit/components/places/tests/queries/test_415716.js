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
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Clint Talbert <ctalbert@mozilla.com>
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

try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
}

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
}

function add_visit(aURI, aDate, aReferrer, aType, isRedirect, aSessionID) {
  var placeID = histsvc.addVisit(aURI,
                                 aDate,
                                 aReferrer,
                                 aType,
                                 isRedirect,
                                 aSessionID);
  do_check_true(placeID > 0);
  return placeID;
}

function modHistoryTypes(val){
  switch(val % 7) {
    case 0:
    case 1:
      return histsvc.TRANSITION_LINK;
    case 2:
      return histsvc.TRANSITION_TYPED;
    case 3:
      return histsvc.TRANSITION_BOOKMARK;
    case 4:
      return histsvc.TRANSITION_EMBED;
    case 5:
      return histsvc.TRANSITION_REDIRECT_PERMANENT;
    case 6:
      return histsvc.TRANSITION_REDIRECT_TEMPORARY;
    case 7:
      return histsvc.TRANSITION_DOWNLOAD;
  }
  return histsvc.TRANSITION_TYPED;
}

/**
 * Builds a test database by hand using various times, annotations and
 * visit numbers for this test
 */
function buildTestDatabase() {
  // This is the set of visits that we will match - our min visit is 2 so that's
  // why we add more visits to the same URIs.
  var testURI = uri("http://www.foo.com");
  var testAnnoName = "moz-test-places/testing123";
  var testAnnoVal = "test";

  for (var i=0; i < 12; ++i)
    add_visit(testURI,
              today,
              null,
              modHistoryTypes(i), // will work with different values, for ex: histsvc.TRANSITION_TYPED,
              false,
              0);

  testURI = uri("http://foo.com/youdontseeme.html");
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, 0);
  for (var i=0; i < 12; ++i)
    add_visit(testURI,
              today,
              null,
              modHistoryTypes(i), // will work with different values, for ex: histsvc.TRANSITION_TYPED,
              false,
              0);
}

/**
 * This test will test Queries that use relative Time Range, minVists, maxVisits,
 * annotation.
 * The Query:
 * Annotation == "moz-test-places/testing123" &&
 * TimeRange == "now() - 2d" &&
 * minVisits == 2 &&
 * maxVisits == 10 
 */
function run_test() {
  buildTestDatabase();
  var query = histsvc.getNewQuery();
  query.annotation = "moz-test-places/testing123";
  query.beginTime = daybefore * 1000;
  query.beginTimeReference = histsvc.TIME_RELATIVE_NOW;
  query.endTime = today * 1000;
  query.endTimeReference = histsvc.TIME_RELATIVE_NOW;
  query.minVisits = 2;
  query.maxVisits = 10;

  // Options
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;

  // Results
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  dump("----> cc is: " + cc + "\n");
  for(var i=0; i < root.childCount; ++i) {
    var resultNode = root.getChild(i);
    var accesstime = Date(resultNode.time);
    dump("----> result: " + resultNode.uri + "   Date: " + accesstime.toLocaleString() + "\n");
  }
  do_check_eq(cc,0);
}
