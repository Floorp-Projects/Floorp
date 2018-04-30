/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var current_test = 0;

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
    QueryInterface: ChromeUtils.generateQI([Ci.nsIAutoCompletePopup])
  },

  // nsISupports implementation
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAutoCompleteInput])
};

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch (ex) {
  do_throw("Could not get tagging service\n");
}

function ensure_tag_results(uris, searchTerm) {
  print("Searching for '" + searchTerm + "'");
  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["unifiedcomplete"]);

  controller.input = input;

  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    Assert.equal(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    Assert.equal(numSearchesStarted, 1);
    Assert.equal(controller.searchStatus,
                 uris.length ?
                 Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH :
                 Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
    Assert.equal(controller.matchCount, uris.length);
    let vals = [];
    for (let i = 0; i < controller.matchCount; i++) {
      // Keep the URL for later because order of tag results is undefined
      vals.push(controller.getValueAt(i));
      Assert.equal(controller.getStyleAt(i), "bookmark-tag");
    }
    // Sort the results then check if we have the right items
    vals.sort().forEach((val, i) => Assert.equal(val, uris[i]));

    if (current_test < (tests.length - 1)) {
      current_test++;
      tests[current_test]();
    }

    do_test_finished();
  };

  controller.startSearch(searchTerm);
}

var uri1 = "http://site.tld/1/aaa";
var uri2 = "http://site.tld/2/bbb";
var uri3 = "http://site.tld/3/aaa";
var uri4 = "http://site.tld/4/bbb";
var uri5 = "http://site.tld/5/aaa";
var uri6 = "http://site.tld/6/bbb";

var tests = [
  () => ensure_tag_results([uri1, uri4, uri6], "foo"),
  () => ensure_tag_results([uri1], "foo aaa"),
  () => ensure_tag_results([uri4, uri6], "foo bbb"),
  () => ensure_tag_results([uri2, uri4, uri5, uri6], "bar"),
  () => ensure_tag_results([uri5], "bar aaa"),
  () => ensure_tag_results([uri2, uri4, uri6], "bar bbb"),
  () => ensure_tag_results([uri3, uri5, uri6], "cheese"),
  () => ensure_tag_results([uri3, uri5], "chees aaa"),
  () => ensure_tag_results([uri6], "chees bbb"),
  () => ensure_tag_results([uri4, uri6], "fo bar"),
  () => ensure_tag_results([], "fo bar aaa"),
  () => ensure_tag_results([uri4, uri6], "fo bar bbb"),
  () => ensure_tag_results([uri4, uri6], "ba foo"),
  () => ensure_tag_results([], "ba foo aaa"),
  () => ensure_tag_results([uri4, uri6], "ba foo bbb"),
  () => ensure_tag_results([uri5, uri6], "ba chee"),
  () => ensure_tag_results([uri5], "ba chee aaa"),
  () => ensure_tag_results([uri6], "ba chee bbb"),
  () => ensure_tag_results([uri5, uri6], "cheese bar"),
  () => ensure_tag_results([uri5], "cheese bar aaa"),
  () => ensure_tag_results([uri6], "chees bar bbb"),
  () => ensure_tag_results([uri6], "cheese bar foo"),
  () => ensure_tag_results([], "foo bar cheese aaa"),
  () => ensure_tag_results([uri6], "foo bar cheese bbb"),
];

/**
 * Properly tags a uri adding it to bookmarks.
 *
 * @param url
 *        The nsIURI to tag.
 * @param tags
 *        The tags to add.
 */
async function tagURI(url, tags) {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: "A title",
  });
  tagssvc.tagURI(uri(url), tags);
}

/**
 * Test history autocomplete
 */
add_task(async function test_history_autocomplete_tags() {
  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setIntPref("browser.urlbar.search.sources", 3);
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 0);

  await tagURI(uri1, ["foo"]);
  await tagURI(uri2, ["bar"]);
  await tagURI(uri3, ["cheese"]);
  await tagURI(uri4, ["foo bar"]);
  await tagURI(uri5, ["bar cheese"]);
  await tagURI(uri6, ["foo bar cheese"]);

  tests[0]();
});
