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

  getSearchAt(aIndex) {
    return this.searches[aIndex];
  },

  onSearchBegin() {},
  onSearchComplete() {},

  popupOpen: false,

  popup: {
    setSelectedIndex(aIndex) {},
    invalidate() {},

    // nsISupports implementation
    QueryInterface(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsISupports implementation
  QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

async function ensure_results(uris, searchTerm) {
  await PlacesTestUtils.promiseAsyncUpdates();
  await ensure_results_internal(uris, searchTerm);
}

async function ensure_results_internal(uris, searchTerm) {
  var controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["unifiedcomplete"]);

  controller.input = input;

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    Assert.equal(numSearchesStarted, 1);
  };

  let promise = new Promise(resolve => {
    input.onSearchComplete = function() {
      Assert.equal(numSearchesStarted, 1);
      Assert.equal(controller.searchStatus,
                   Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
      Assert.equal(controller.matchCount, uris.length);
      for (var i = 0; i < controller.matchCount; i++) {
        Assert.equal(controller.getValueAt(i), uris[i].spec);
      }

      resolve();
    };
  });

  controller.startSearch(searchTerm);
  await promise;
}

// Get history service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch (ex) {
  do_throw("Could not get history service\n");
}

async function task_setCountDate(aURI, aCount, aDate) {
  // We need visits so that frecency can be computed over multiple visits
  let visits = [];
  for (let i = 0; i < aCount; i++) {
    visits.push({ uri: aURI, visitDate: aDate, transition: TRANSITION_TYPED });
  }
  await PlacesTestUtils.addVisits(visits);
}

async function setBookmark(aURI) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: aURI,
    title: "bleh"
  });
}

async function tagURI(aURI, aTags) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: aURI,
    title: "bleh",
  });
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
async function() {
  print("TEST-INFO | Test 0: same count, different date");
  await task_setCountDate(uri1, c1, d1);
  await task_setCountDate(uri2, c1, d2);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri1, uri2], "");
},
async function() {
  print("TEST-INFO | Test 1: same count, different date");
  await task_setCountDate(uri1, c1, d2);
  await task_setCountDate(uri2, c1, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri2, uri1], "");
},
async function() {
  print("TEST-INFO | Test 2: different count, same date");
  await task_setCountDate(uri1, c1, d1);
  await task_setCountDate(uri2, c2, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri1, uri2], "");
},
async function() {
  print("TEST-INFO | Test 3: different count, same date");
  await task_setCountDate(uri1, c2, d1);
  await task_setCountDate(uri2, c1, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri2, uri1], "");
},

// test things with a search term
async function() {
  print("TEST-INFO | Test 4: same count, different date");
  await task_setCountDate(uri1, c1, d1);
  await task_setCountDate(uri2, c1, d2);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri1, uri2], "site");
},
async function() {
  print("TEST-INFO | Test 5: same count, different date");
  await task_setCountDate(uri1, c1, d2);
  await task_setCountDate(uri2, c1, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri2, uri1], "site");
},
async function() {
  print("TEST-INFO | Test 6: different count, same date");
  await task_setCountDate(uri1, c1, d1);
  await task_setCountDate(uri2, c2, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri1, uri2], "site");
},
async function() {
  print("TEST-INFO | Test 7: different count, same date");
  await task_setCountDate(uri1, c2, d1);
  await task_setCountDate(uri2, c1, d1);
  await tagURI(uri1, ["site"]);
  await ensure_results([uri2, uri1], "site");
},
// There are multiple tests for 8, hence the multiple functions
// Bug 426166 section
async function() {
  print("TEST-INFO | Test 8.1a: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "a");
},
async function() {
  print("TEST-INFO | Test 8.1b: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "aa");
},
async function() {
  print("TEST-INFO | Test 8.2: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "aaa");
},
async function() {
  print("TEST-INFO | Test 8.3: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "aaaa");
},
async function() {
  print("TEST-INFO | Test 8.4: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "aaa");
},
async function() {
  print("TEST-INFO | Test 8.5: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "aa");
},
async function() {
  print("TEST-INFO | Test 8.6: same count, same date");
  await setBookmark(uri3);
  await setBookmark(uri4);
  await ensure_results([uri4, uri3], "a");
}
];

add_task(async function test_frecency() {
  // Disable autoFill for this test.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  registerCleanupFunction(() => Services.prefs.clearUserPref("browser.urlbar.autoFill"));
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  for (let test of tests) {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesTestUtils.clearHistory();

    await test();
  }
  for (let type of ["history", "bookmark", "openpage"]) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
});
