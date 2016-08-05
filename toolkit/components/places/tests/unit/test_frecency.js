/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 406358 to make sure frecency works for empty input/search, but
 * this also tests for non-empty inputs as well. Because the interactions among
 * *DIFFERENT* visit counts and visit dates is not well defined, this test
 * holds one of the two values constant when modifying the other.
 *
 * Also test bug 419068 to make sure tagged pages don't necessarily have to be
 * first in the results.
 *
 * Also test bug 426166 to make sure that the results of autocomplete searches
 * are stable.  Note that failures of this test will be intermittent by nature
 * since we are testing to make sure that the unstable sort algorithm used
 * by SQLite is not changing the order of the results on us.
 */

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput,

  searches: null,

  minResultsForPopup: 0,
  timeout: 10,
  searchParam: "",
  textValue: "",
  disableAutoComplete: false,
  completeDefaultIndex: false,

  get searchCount() {
    return this.searches.length;
  },

  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },

  onSearchBegin: function() {},
  onSearchComplete: function() {},

  popupOpen: false,

  popup: {
    setSelectedIndex: function(aIndex) {},
    invalidate: function() {},

    // nsISupports implementation
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsISupports implementation
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

function ensure_results(uris, searchTerm)
{
  PlacesTestUtils.promiseAsyncUpdates()
                 .then(() => ensure_results_internal(uris, searchTerm));
}

function ensure_results_internal(uris, searchTerm)
{
  var controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["unifiedcomplete"]);

  controller.input = input;

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    do_check_eq(numSearchesStarted, 1);
    do_check_eq(controller.searchStatus,
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
    do_check_eq(controller.matchCount, uris.length);
    for (var i=0; i<controller.matchCount; i++) {
      do_check_eq(controller.getValueAt(i), uris[i].spec);
    }

    deferEnsureResults.resolve();
  };

  controller.startSearch(searchTerm);
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
  var bmksvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);
} catch (ex) {
  do_throw("Could not get history service\n");
}

function* task_setCountDate(aURI, aCount, aDate)
{
  // We need visits so that frecency can be computed over multiple visits
  let visits = [];
  for (let i = 0; i < aCount; i++) {
    visits.push({ uri: aURI, visitDate: aDate, transition: TRANSITION_TYPED });
  }
  yield PlacesTestUtils.addVisits(visits);
}

function setBookmark(aURI)
{
  bmksvc.insertBookmark(bmksvc.bookmarksMenuFolder, aURI, -1, "bleh");
}

function tagURI(aURI, aTags) {
  bmksvc.insertBookmark(bmksvc.unfiledBookmarksFolder, aURI,
                        bmksvc.DEFAULT_INDEX, "bleh");
  tagssvc.tagURI(aURI, aTags);
}

var uri1 = uri("http://site.tld/1");
var uri2 = uri("http://site.tld/2");
var uri3 = uri("http://aaaaaaaaaa/1");
var uri4 = uri("http://aaaaaaaaaa/2");

// d1 is younger (should show up higher) than d2 (PRTime is in usecs not msec)
// Make sure the dates fall into different frecency buckets
var d1 = new Date(Date.now() - 1000 * 60 * 60) * 1000;
var d2 = new Date(Date.now() - 1000 * 60 * 60 * 24 * 10) * 1000;
// c1 is larger (should show up higher) than c2
var c1 = 10;
var c2 = 1;

var tests = [
// test things without a search term
function*() {
  print("TEST-INFO | Test 0: same count, different date");
  yield task_setCountDate(uri1, c1, d1);
  yield task_setCountDate(uri2, c1, d2);
  tagURI(uri1, ["site"]);
  ensure_results([uri1, uri2], "");
},
function*() {
  print("TEST-INFO | Test 1: same count, different date");
  yield task_setCountDate(uri1, c1, d2);
  yield task_setCountDate(uri2, c1, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri2, uri1], "");
},
function*() {
  print("TEST-INFO | Test 2: different count, same date");
  yield task_setCountDate(uri1, c1, d1);
  yield task_setCountDate(uri2, c2, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri1, uri2], "");
},
function*() {
  print("TEST-INFO | Test 3: different count, same date");
  yield task_setCountDate(uri1, c2, d1);
  yield task_setCountDate(uri2, c1, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri2, uri1], "");
},

// test things with a search term
function*() {
  print("TEST-INFO | Test 4: same count, different date");
  yield task_setCountDate(uri1, c1, d1);
  yield task_setCountDate(uri2, c1, d2);
  tagURI(uri1, ["site"]);
  ensure_results([uri1, uri2], "site");
},
function*() {
  print("TEST-INFO | Test 5: same count, different date");
  yield task_setCountDate(uri1, c1, d2);
  yield task_setCountDate(uri2, c1, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri2, uri1], "site");
},
function*() {
  print("TEST-INFO | Test 6: different count, same date");
  yield task_setCountDate(uri1, c1, d1);
  yield task_setCountDate(uri2, c2, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri1, uri2], "site");
},
function*() {
  print("TEST-INFO | Test 7: different count, same date");
  yield task_setCountDate(uri1, c2, d1);
  yield task_setCountDate(uri2, c1, d1);
  tagURI(uri1, ["site"]);
  ensure_results([uri2, uri1], "site");
},
// There are multiple tests for 8, hence the multiple functions
// Bug 426166 section
function*() {
  print("TEST-INFO | Test 8.1a: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "a");
},
function*() {
  print("TEST-INFO | Test 8.1b: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "aa");
},
function*() {
  print("TEST-INFO | Test 8.2: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "aaa");
},
function*() {
  print("TEST-INFO | Test 8.3: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "aaaa");
},
function*() {
  print("TEST-INFO | Test 8.4: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "aaa");
},
function*() {
  print("TEST-INFO | Test 8.5: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "aa");
},
function*() {
  print("TEST-INFO | Test 8.6: same count, same date");
  setBookmark(uri3);
  setBookmark(uri4);
  ensure_results([uri4, uri3], "a");
}
];

/**
 * This deferred object contains a promise that is resolved when the
 * ensure_results_internal function has finished its execution.
 */
var deferEnsureResults;

add_task(function* test_frecency()
{
  // Disable autoFill for this test.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  do_register_cleanup(() => Services.prefs.clearUserPref("browser.urlbar.autoFill"));
  // always search in history + bookmarks, no matter what the default is
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("browser.urlbar.suggest.history", true);
  prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  for (let test of tests) {
    yield PlacesUtils.bookmarks.eraseEverything();
    yield PlacesTestUtils.clearHistory();

    deferEnsureResults = Promise.defer();
    yield test();
    yield deferEnsureResults.promise;
  }
  for (let type of ["history", "bookmark", "openpage"]) {
    prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
});
