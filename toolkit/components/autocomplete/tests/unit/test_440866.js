/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Unit test for Bug 440866 - First AutoCompleteSearch that returns
 * RESULT_NOMATCH cancels all other searches when popup is open
 */

/**
 * Dummy nsIAutoCompleteInput source that returns
 * the given list of AutoCompleteSearch names.
 *
 * Implements only the methods needed for this test.
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  constructor: AutoCompleteInput,

  // Array of AutoCompleteSearch names
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

/**
 * nsIAutoCompleteResult implementation
 */
function AutoCompleteResult(aValues, aComments, aStyles) {
  this._values = aValues;
  this._comments = aComments;
  this._styles = aStyles;

  if (this._values.length) {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
  } else {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }
}
AutoCompleteResult.prototype = {
  constructor: AutoCompleteResult,

  // Arrays
  _values: null,
  _comments: null,
  _styles: null,

  searchString: "",
  searchResult: null,

  defaultIndex: 0,

  get matchCount() {
    return this._values.length;
  },

  getValueAt(aIndex) {
    return this._values[aIndex];
  },

  getLabelAt(aIndex) {
    return this.getValueAt(aIndex);
  },

  getCommentAt(aIndex) {
    return this._comments[aIndex];
  },

  getStyleAt(aIndex) {
    return this._styles[aIndex];
  },

  getImageAt(aIndex) {
    return "";
  },

  getFinalCompleteValueAt(aIndex) {
    return this.getValueAt(aIndex);
  },

  isRemovableAt(aRowIndex) {
    return true;
  },

  removeValueAt(aRowIndex) {},

  // nsISupports implementation
  QueryInterface: ChromeUtils.generateQI(["nsIAutoCompleteResult"]),
};

/**
 * nsIAutoCompleteSearch implementation that always returns
 * the same result set.
 */
function AutoCompleteSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteSearch.prototype = {
  constructor: AutoCompleteSearch,

  // Search name. Used by AutoCompleteController
  name: null,

  // AutoCompleteResult
  _result: null,

  /**
   * Return the same result set for every search
   */
  startSearch(aSearchString, aSearchParam, aPreviousResult, aListener) {
    aListener.onSearchResult(this, this._result);
  },

  stopSearch() {},

  // nsISupports implementation
  QueryInterface: ChromeUtils.generateQI([
    "nsIFactory",
    "nsIAutoCompleteSearch",
  ]),

  // nsIFactory implementation
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },
};

/**
 * Helper to register an AutoCompleteSearch with the given name.
 * Allows the AutoCompleteController to find the search.
 */
function registerAutoCompleteSearch(aSearch) {
  var name = "@mozilla.org/autocomplete/search;1?name=" + aSearch.name;

  var uuidGenerator = Services.uuid;
  var cid = uuidGenerator.generateUUID();

  var desc = "Test AutoCompleteSearch";

  var componentManager = Components.manager.QueryInterface(
    Ci.nsIComponentRegistrar
  );
  componentManager.registerFactory(cid, desc, name, aSearch);

  // Keep the id on the object so we can unregister later
  aSearch.cid = cid;
}

/**
 * Helper to unregister an AutoCompleteSearch.
 */
function unregisterAutoCompleteSearch(aSearch) {
  var componentManager = Components.manager.QueryInterface(
    Ci.nsIComponentRegistrar
  );
  componentManager.unregisterFactory(aSearch.cid, aSearch);
}

/**
 * Test AutoComplete with multiple AutoCompleteSearch sources.
 */
function run_test() {
  // Make an AutoCompleteSearch that always returns nothing
  var emptySearch = new AutoCompleteSearch(
    "test-empty-search",
    new AutoCompleteResult([], [], [])
  );

  // Make an AutoCompleteSearch that returns two values
  var expectedValues = ["test1", "test2"];
  var regularSearch = new AutoCompleteSearch(
    "test-regular-search",
    new AutoCompleteResult(expectedValues, [], [])
  );

  // Register searches so AutoCompleteController can find them
  registerAutoCompleteSearch(emptySearch);
  registerAutoCompleteSearch(regularSearch);

  var controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput([emptySearch.name, regularSearch.name]);
  var numSearchesStarted = 0;

  input.onSearchBegin = function() {
    numSearchesStarted++;
    Assert.equal(numSearchesStarted, 1);
    Assert.equal(input.searchCount, 2);
  };

  input.onSearchComplete = function() {
    Assert.equal(numSearchesStarted, 1);

    Assert.equal(
      controller.searchStatus,
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH
    );
    Assert.equal(controller.matchCount, 2);

    // Confirm expected result values
    for (var i = 0; i < expectedValues.length; i++) {
      Assert.equal(expectedValues[i], controller.getValueAt(i));
    }

    Assert.ok(input.popupOpen);

    // Unregister searches
    unregisterAutoCompleteSearch(emptySearch);
    unregisterAutoCompleteSearch(regularSearch);

    do_test_finished();
  };

  controller.input = input;

  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  controller.startSearch("test");
}
