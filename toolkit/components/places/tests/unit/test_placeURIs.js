/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// main
function run_test() {
  // XXX Full testing coverage for QueriesToQueryString and
  // QueryStringToQueries

  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  const NHQO = Ci.nsINavHistoryQueryOptions;
  // Bug 376798
  var query = histsvc.getNewQuery();
  query.setFolders([bs.placesRoot], 1);
  do_check_eq(histsvc.queriesToQueryString([query], 1, histsvc.getNewQueryOptions()),
              "place:folder=PLACES_ROOT");

  // Bug 378828
  var options = histsvc.getNewQueryOptions();
  options.sortingAnnotation = "test anno";
  options.sortingMode = NHQO.SORT_BY_ANNOTATION_DESCENDING;
  var placeURI =
    "place:folder=PLACES_ROOT&sort=" + NHQO.SORT_BY_ANNOTATION_DESCENDING +
    "&sortingAnnotation=test%20anno";
  do_check_eq(histsvc.queriesToQueryString([query], 1, options),
              placeURI);
  var options = {};
  histsvc.queryStringToQueries(placeURI, { }, {}, options);
  do_check_eq(options.value.sortingAnnotation, "test anno");
  do_check_eq(options.value.sortingMode, NHQO.SORT_BY_ANNOTATION_DESCENDING);
}
