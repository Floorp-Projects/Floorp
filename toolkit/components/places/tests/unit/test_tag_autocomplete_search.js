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
    QueryInterface: ChromeUtils.generateQI(["nsIAutoCompletePopup"])
  },

  // nsISupports implementation
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteInput"])
};

async function ensure_tag_results(results, searchTerm) {
  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["places-tag-autocomplete"]);

  controller.input = input;

  return new Promise(resolve => {
    var numSearchesStarted = 0;
    input.onSearchBegin = function input_onSearchBegin() {
      numSearchesStarted++;
      Assert.equal(numSearchesStarted, 1);
    };

    input.onSearchComplete = function input_onSearchComplete() {
      Assert.equal(numSearchesStarted, 1);
      if (results.length)
        Assert.equal(controller.searchStatus,
                     Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
      else
        Assert.equal(controller.searchStatus,
                     Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);

      Assert.equal(controller.matchCount, results.length);
      for (var i = 0; i < controller.matchCount; i++) {
        Assert.equal(controller.getValueAt(i), results[i]);
      }

      resolve();
    };

    controller.startSearch(searchTerm);
  });
}

var uri1 = uri("http://site.tld/1");

var tests = [
  () => ensure_tag_results(["bar", "Baz", "boo"], "b"),
  () => ensure_tag_results(["bar", "Baz"], "ba"),
  () => ensure_tag_results(["bar", "Baz"], "Ba"),
  () => ensure_tag_results(["bar"], "bar"),
  () => ensure_tag_results(["Baz"], "Baz"),
  () => ensure_tag_results([], "barb"),
  () => ensure_tag_results([], "foo"),
  () => ensure_tag_results(["first tag, bar", "first tag, Baz"], "first tag, ba"),
  () => ensure_tag_results(["first tag;  bar", "first tag;  Baz"], "first tag;  ba"),
];

/**
 * Test tag autocomplete
 */
add_task(async function test_tag_autocomplete() {
  PlacesUtils.tagging.tagURI(uri1, ["bar", "Baz", "boo", "*nix"]);

  for (let tagTest of tests) {
    await tagTest();
  }
});
