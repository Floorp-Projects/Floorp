/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  get minResultsForPopup() {
    return 0;
  },
  get timeout() {
    return 10;
  },
  get searchParam() {
    return "";
  },
  get textValue() {
    return "";
  },
  get disableAutoComplete() {
    return false;
  },
  get completeDefaultIndex() {
    return false;
  },

  get searchCount() {
    return this.searches.length;
  },
  getSearchAt: function (aIndex) {
    return this.searches[aIndex];
  },

  onSearchBegin: function () {},
  onSearchComplete: function() {},

  get popupOpen() {
    return false;
  },
  popup: {
    set selectedIndex(aIndex) {},
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
  let input = new AutoCompleteInput(["unifiedcomplete"]);

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

    deferEnsureResults.resolve();
  };

  controller.startSearch(searchTerm);
}

/**
 * Asynchronous task that bumps up the rank for an uri.
 */
function* task_setCountRank(aURI, aCount, aRank, aSearch, aBookmark)
{
  // Bump up the visit count for the uri.
  let visits = [];
  for (let i = 0; i < aCount; i++) {
    visits.push({ uri: aURI, visitDate: d1, transition: TRANSITION_TYPED });
  }
  yield PlacesTestUtils.addVisits(visits);

  // Make a nsIAutoCompleteController and friends for instrumentation feedback.
  let thing = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput,
                                           Ci.nsIAutoCompletePopup,
                                           Ci.nsIAutoCompleteController]),
    get popup() {
      return thing;
    },
    get controller() {
      return thing;
    },
    popupOpen: true,
    selectedIndex: 0,
    getValueAt: function() {
      return aURI.spec;
    },
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

var uri1 = uri("http://site.tld/1");
var uri2 = uri("http://site.tld/2");

// d1 is some date for the page visit
var d1 = new Date(Date.now() - 1000 * 60 * 60) * 1000;
// c1 is larger (should show up higher) than c2
var c1 = 10;
var c2 = 1;
// s1 is a partial match of s2
var s0 = "";
var s1 = "si";
var s2 = "site";

var observer = {
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
function makeResult(aURI, aStyle = "favicon") {
  return {
    uri: aURI,
    style: aStyle,
  };
}

var tests = [
  // Test things without a search term.
  function*() {
    print("Test 0 same count, diff rank, same term; no search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c1, s2);
    yield task_setCountRank(uri2, c1, c2, s2);
  },
  function*() {
    print("Test 1 same count, diff rank, same term; no search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c2, s2);
    yield task_setCountRank(uri2, c1, c1, s2);
  },
  function*() {
    print("Test 2 diff count, same rank, same term; no search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri1, c1, c1, s2);
    yield task_setCountRank(uri2, c2, c1, s2);
  },
  function*() {
    print("Test 3 diff count, same rank, same term; no search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s0;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri1, c2, c1, s2);
    yield task_setCountRank(uri2, c1, c1, s2);
  },

  // Test things with a search term (exact match one, partial other).
  function*() {
    print("Test 4 same count, same rank, diff term; one exact/one partial search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri1, c1, c1, s1);
    yield task_setCountRank(uri2, c1, c1, s2);
  },
  function*() {
    print("Test 5 same count, same rank, diff term; one exact/one partial search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri1, c1, c1, s2);
    yield task_setCountRank(uri2, c1, c1, s1);
  },

  // Test things with a search term (exact match both).
  function*() {
    print("Test 6 same count, diff rank, same term; both exact search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c1, s1);
    yield task_setCountRank(uri2, c1, c2, s1);
  },
  function*() {
    print("Test 7 same count, diff rank, same term; both exact search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c2, s1);
    yield task_setCountRank(uri2, c1, c1, s1);
  },

  // Test things with a search term (partial match both).
  function*() {
    print("Test 8 same count, diff rank, same term; both partial search");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c1, s2);
    yield task_setCountRank(uri2, c1, c2, s2);
  },
  function*() {
    print("Test 9 same count, diff rank, same term; both partial search");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c2, s2);
    yield task_setCountRank(uri2, c1, c1, s2);
  },
  function*() {
    print("Test 10 same count, same rank, same term, decay first; exact match");
    observer.results = [
      makeResult(uri2),
      makeResult(uri1),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri1, c1, c1, s1);
    doAdaptiveDecay();
    yield task_setCountRank(uri2, c1, c1, s1);
  },
  function*() {
    print("Test 11 same count, same rank, same term, decay second; exact match");
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s1;
    observer.runCount = c1 + c1;
    yield task_setCountRank(uri2, c1, c1, s1);
    doAdaptiveDecay();
    yield task_setCountRank(uri1, c1, c1, s1);
  },
  // Test that bookmarks are hidden if the preferences are set right.
  function*() {
    print("Test 12 same count, diff rank, same term; no search; history only");
    Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
    observer.results = [
      makeResult(uri1),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c1, s2, "bookmark");
    yield task_setCountRank(uri2, c1, c2, s2);
  },
  // Test that tags are shown if the preferences are set right.
  function*() {
    print("Test 13 same count, diff rank, same term; no search; history only with tag");
    Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
    Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
    Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
    observer.results = [
      makeResult(uri1, "tag"),
      makeResult(uri2),
    ];
    observer.search = s0;
    observer.runCount = c1 + c2;
    yield task_setCountRank(uri1, c1, c1, s2, "tag");
    yield task_setCountRank(uri2, c1, c2, s2);
  },
];

/**
 * This deferred object contains a promise that is resolved when the
 * ensure_results function has finished its execution.
 */
var deferEnsureResults;

/**
 * Test adaptive autocomplete.
 */
add_task(function* test_adaptive()
{
  // Disable autoFill for this test.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  do_register_cleanup(() => Services.prefs.clearUserPref("browser.urlbar.autoFill"));
  for (let [, test] in Iterator(tests)) {
    // Cleanup.
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.tagsFolderId);
    observer.runCount = -1;

    let types = ["history", "bookmark", "openpage"];
    for (let type of types) {
      Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
    }

    yield PlacesTestUtils.clearHistory();

    deferEnsureResults = Promise.defer();
    yield test();
    yield deferEnsureResults.promise;
  }

  Services.obs.removeObserver(observer, PlacesUtils.TOPIC_FEEDBACK_UPDATED);
});
