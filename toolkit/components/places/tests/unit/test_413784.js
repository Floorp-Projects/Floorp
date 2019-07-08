/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

Test autocomplete for non-English URLs

- add a visit for a page with a non-English URL
- search
- test number of matches (should be exactly one)

*/

// create test data
var searchTerm = "ユニコード";
var decoded = "http://www.foobar.com/" + searchTerm + "/";
var url = uri(decoded);

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
    QueryInterface: ChromeUtils.generateQI(["nsIAutoCompletePopup"]),
  },

  // nsISupports implementation
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteInput"]),
};

add_task(async function test_autocomplete_non_english() {
  await PlacesTestUtils.addVisits(url);

  var controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["unifiedcomplete"]);

  controller.input = input;

  return new Promise(resolve => {
    var numSearchesStarted = 0;
    input.onSearchBegin = function() {
      numSearchesStarted++;
      Assert.equal(numSearchesStarted, 1);
    };

    input.onSearchComplete = function() {
      Assert.equal(numSearchesStarted, 1);
      Assert.equal(
        controller.searchStatus,
        Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH
      );

      // test that we found the entry we added
      Assert.equal(controller.matchCount, 1);

      // Make sure the url is the same according to spec, so it can be deleted
      Assert.equal(controller.getValueAt(0), url.spec);

      resolve();
    };

    controller.startSearch(searchTerm);
  });
});
