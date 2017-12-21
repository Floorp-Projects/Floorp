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

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch (ex) {
  do_throw("Could not get tagging service\n");
}

function ensure_tag_results(results, searchTerm) {
  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["places-tag-autocomplete"]);

  controller.input = input;

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

    if (current_test < (tests.length - 1)) {
      current_test++;
      tests[current_test]();
    } else {
      // finish once all tests have run
      do_test_finished();
    }
  };

  controller.startSearch(searchTerm);
}

var uri1 = uri("http://site.tld/1");

var tests = [
  function test1() { ensure_tag_results(["bar", "Baz", "boo"], "b"); },
  function test2() { ensure_tag_results(["bar", "Baz"], "ba"); },
  function test3() { ensure_tag_results(["bar", "Baz"], "Ba"); },
  function test4() { ensure_tag_results(["bar"], "bar"); },
  function test5() { ensure_tag_results(["Baz"], "Baz"); },
  function test6() { ensure_tag_results([], "barb"); },
  function test7() { ensure_tag_results([], "foo"); },
  function test8() { ensure_tag_results(["first tag, bar", "first tag, Baz"], "first tag, ba"); },
  function test9() { ensure_tag_results(["first tag;  bar", "first tag;  Baz"], "first tag;  ba"); }
];

/**
 * Test tag autocomplete
 */
function run_test() {
  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  tagssvc.tagURI(uri1, ["bar", "Baz", "boo", "*nix"]);

  tests[0]();
}
