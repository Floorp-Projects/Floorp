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
 * The Original Code is Bug 378079 unit test code.
 *
 * The Initial Developer of the Original Code is POTI Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Crocker <matt@songbirdnest.com>
 *   Seth Spitzer <sspitzer@mozilla.org>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
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

/**
 * Test for bug 395739 to make sure the feedback to the search results in those
 * entries getting better ranks. Additionally, exact matches should be ranked
 * higher. Because the interactions among adaptive rank and visit counts is not
 * well defined, this test holds one of the two values constant when modifying
 * the other.
 *
 * This also tests bug 395735 for the instrumentation feedback mechanism.
 *
 * Bug 411293 is tested to make sure the drop down strongly prefers previously
 * typed pages that have been selected and are moved to the top with adaptive
 * learning.
 */

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput,

  get minResultsForPopup() 0,
  get timeout() 10,
  get searchParam() "",
  get textValue() "",
  get disableAutoComplete() false,
  get completeDefaultIndex() false,

  get searchCount() this.searches.length,
  getSearchAt: function (aIndex) this.searches[aIndex],

  onSearchBegin: function () {},
  onSearchComplete: function() {},

  get popupOpen() false,
  popup: {
    set selectedIndex(aIndex) aIndex,
    invalidate: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup])
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput])
}

/**
 * Checks that autocomplete results are ordered correctly.
 */
function ensure_results(expected, searchTerm)
{
  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete.
  let input = new AutoCompleteInput(["history"]);

  controller.input = input;

  input.onSearchComplete = function() {
    do_check_eq(controller.searchStatus,
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
    do_check_eq(controller.matchCount, expected.length);
    for (let i = 0; i < controller.matchCount; i++) {
      print("Testing for '" + expected[i].uri.spec + "' got '" + controller.getValueAt(i) + "'");
      do_check_eq(controller.getValueAt(i), expected[i].uri.spec);
      do_check_eq(controller.getStyleAt(i), expected[i].style);
    }

    next_test();
  };

  controller.startSearch(searchTerm);
}

/**
 * Bump up the rank for an uri.
 */
function setCountRank(aURI, aCount, aRank, aSearch, aBookmark)
{
  PlacesUtils.history.runInBatchMode({
    runBatched: function() {
      // Bump up the visit count for the uri.
      for (let i = 0; i < aCount; i++) {
        PlacesUtils.history.addVisit(aURI, d1, null,
                                     PlacesUtils.history.TRANSITION_TYPED,
                                     false, 0);
      }
    }
  }, this);

  // Make a nsIAutoCompleteController and friends for instrumentation feedback.
  let thing = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput,
                                           Ci.nsIAutoCompletePopup,
                                           Ci.nsIAutoCompleteController]),
    get popup() thing,
    get controller() thing,
    popupOpen: true,
    selectedIndex: 0,
    getValueAt: function() aURI.spec,
    searchString: aSearch
  };

  // Bump up the instrumentation feedback.
  for (let i = 0; i < aRank; i++) {
    Services.obs.notifyObservers(thing, "autocomplete-will-enter-text", null);
  }

  // If this is supposed to be a bookmark, add it.
  if (aBookmark) {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         aURI,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test_book");

    // And add the tag if we need to.
    if (aBookmark == "tag") {
      PlacesUtils.tagging.tagURI(aURI, ["test_tag"]);
    }
  }
}

/**
 * Decay the adaptive entries by sending the daily idle topic.
 */
function doAdaptiveDecay()
{
  PlacesUtils.history.runInBatchMode({
    runBatched: function() {
      for (let i = 0; i < 10; i++) {
        PlacesUtils.history.QueryInterface(Ci.nsIObserver)
                           .observe(null, "idle-daily", null);
      }
    }
  }, this);
}

let uri1 = uri("http://site.tld/1");
let uri2 = uri("http://site.tld/2");

// d1 is some date for the page visit
let d1 = new Date(Date.now() - 1000 * 60 * 60) * 1000;
// c1 is larger (should show up higher) than c2
let c1 = 10;
let c2 = 1;
// s1 is a partial match of s2
let s0 = "";
let s1 = "si";
let s2 = "site";

let observer = {
  results: null,
  search: null,
  runCount: -1,
  observe: function(aSubject, aTopic, aData)
  {
    if (--this.runCount > 0)
      return;
    ensure_results(this.results, this.search);
  }
};
Services.obs.addObserver(observer, PlacesUtils.TOPIC_FEEDBACK_UPDATED, false);

/**
 * Make the result object for a given URI that will be passed to ensure_results.
 */
function makeResult(aURI) {
  return {
    uri: aURI,
    style: "favicon",
  };
}

let tests = [
  // Test things without a search term.
  function() {
    print("Test 0 same count, diff rank, same term; no search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c1, s2);
    setCountRank(uri2, c1, c2, s2);
  },
  function() {
    print("Test 1 same count, diff rank, same term; no search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c2, s2);
    setCountRank(uri2, c1, c1, s2);
  },
  function() {
    print("Test 2 diff count, same rank, same term; no search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c1;
    setCountRank(uri1, c1, c1, s2);
    setCountRank(uri2, c2, c1, s2);
  },
  function() {
    print("Test 3 diff count, same rank, same term; no search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s0;
    observer.runCount = c1 + c1;
    setCountRank(uri1, c2, c1, s2);
    setCountRank(uri2, c1, c1, s2);
  },

  // Test things with a search term (exact match one, partial other).
  function() {
    print("Test 4 same count, same rank, diff term; one exact/one partial search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    setCountRank(uri1, c1, c1, s1);
    setCountRank(uri2, c1, c1, s2);
  },
  function() {
    print("Test 5 same count, same rank, diff term; one exact/one partial search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    setCountRank(uri1, c1, c1, s2);
    setCountRank(uri2, c1, c1, s1);
  },

  // Test things with a search term (exact match both).
  function() {
    print("Test 6 same count, diff rank, same term; both exact search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c1, s1);
    setCountRank(uri2, c1, c2, s1);
  },
  function() {
    print("Test 7 same count, diff rank, same term; both exact search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c2, s1);
    setCountRank(uri2, c1, c1, s1);
  },

  // Test things with a search term (partial match both).
  function() {
    print("Test 8 same count, diff rank, same term; both partial search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c1, s2);
    setCountRank(uri2, c1, c2, s2);
  },
  function() {
    print("Test 9 same count, diff rank, same term; both partial search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c2, s2);
    setCountRank(uri2, c1, c1, s2);
  },
  function() {
    print("Test 10 same count, same rank, same term, decay first; exact match");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    setCountRank(uri1, c1, c1, s1);
    doAdaptiveDecay();
    setCountRank(uri2, c1, c1, s1);
  },
  function() {
    print("Test 11 same count, same rank, same term, decay second; exact match");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    setCountRank(uri2, c1, c1, s1);
    doAdaptiveDecay();
    setCountRank(uri1, c1, c1, s1);
  },
  // Test that bookmarks or tags are hidden if the preferences are set right.
  function() {
    print("Test 12 same count, diff rank, same term; no search; history only");
    Services.prefs.setIntPref("browser.urlbar.matchBehavior",
                              Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY);
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c1, s2, "bookmark");
    setCountRank(uri2, c1, c2, s2);
  },
  function() {
    print("Test 13 same count, diff rank, same term; no search; history only with tag");
    Services.prefs.setIntPref("browser.urlbar.matchBehavior",
                              Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY);
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    setCountRank(uri1, c1, c1, s2, "tag");
    setCountRank(uri2, c1, c2, s2);
  },
];

/**
 * Test adaptive autocomplete.
 */
function run_test() {
  // doAdaptiveDecay notifies idle-daily to fix frecency.  Unfortunately this
  // also causes a vacuum at each iteration.  Thus disable vacuum for this test.
  Services.prefs.setIntPref("places.last_vacuum", parseInt(Date.now()/1000));

  do_test_pending();
  next_test();
}

function next_test() {
  if (tests.length) {
    // Cleanup.
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.tagsFolderId);
    observer.runCount = -1;

    let test = tests.shift();
    waitForClearHistory(test);
  }
  else {
    Services.obs.removeObserver(observer, PlacesUtils.TOPIC_FEEDBACK_UPDATED);
    do_test_finished();
  }
}
