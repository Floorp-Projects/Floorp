/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
let (commonFile = do_get_file("../head_common.js", false)) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

/**
 * @param aSearches
 *        Array of AutoCompleteSearch names. 
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  searches: null,
  minResultsForPopup: 0,
  timeout: 10,
  searchParam: "",
  textValue: "",
  disableAutoComplete: false,

  completeDefaultIndex: true,
  defaultIndex: 0,

  // Text selection range
  _selStart: 0,
  _selEnd: 0,
  get selectionStart() {
    return this._selStart;
  },
  get selectionEnd() {
    return this._selEnd;
  },
  selectTextRange: function(aStart, aEnd) {
    this._selStart = aStart;
    this._selEnd = aEnd;
  },

  get searchCount() {
    return this.searches.length;
  },
  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },

  onSearchBegin: function () {},
  onSearchComplete: function () {},

  popupOpen: false,

  popup: {
    selectedIndex: 0,
    invalidate: function () {},

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup])
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput])
}

/**
 * @param aSearchString
 *        String to search.
 * @param aExpectedValue
 *        Expected value returned by autoFill.
 */
function ensure_results(aSearchString, aExpectedValue) {
  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput(["urlinline"]);
  input.textValue = aSearchString;

  // Caret must be at the end for autoFill to happen.
  let strLen = aSearchString.length;
  input.selectTextRange(strLen, strLen);
  do_check_eq(input.selectionStart, strLen);
  do_check_eq(input.selectionEnd, strLen);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = input;

  let numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    // We should be running only one query.
    do_check_eq(numSearchesStarted, 1);

    // Check the autoFilled result.
    do_check_eq(input.textValue, aExpectedValue);

    waitForCleanup(run_next_test);
  };

  do_log_info("Searching for: '" + aSearchString + "'");
  controller.startSearch(aSearchString);
}

function run_test() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);
  do_register_cleanup(function () {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
  });

  gAutoCompleteTests.forEach(function (testData) {
    let [description, searchString, expectedValue, setupFunc] = testData;
    add_test(function () {
      do_log_info(description);
      if (setupFunc) {
        setupFunc();
      }

      // At this point frecency could still be updating due to latest pages
      // updates.
      // This is not a problem in real life, but autocomplete tests should
      // return reliable resultsets, thus we have to wait.
      waitForAsyncUpdates(ensure_results, this, [searchString, expectedValue]);
    })
  }, this);

  run_next_test();
}

let gAutoCompleteTests = [];
function add_autocomplete_test(aTestData) {
  gAutoCompleteTests.push(aTestData);
}

function waitForCleanup(aCallback) {
  remove_all_bookmarks();
  waitForClearHistory(aCallback);
}

function addBookmark(aBookmarkObj) {
  do_check_true(!!aBookmarkObj.url);
  let parentId = aBookmarkObj.parentId ? aBookmarkObj.parentId
                                       : PlacesUtils.unfiledBookmarksFolderId;
  let itemId = PlacesUtils.bookmarks
                          .insertBookmark(parentId,
                                          NetUtil.newURI(aBookmarkObj.url),
                                          PlacesUtils.bookmarks.DEFAULT_INDEX,
                                          "A bookmark");
  if (aBookmarkObj.keyword) {
    PlacesUtils.bookmarks.setKeywordForBookmark(itemId, aBookmarkObj.keyword);
  }
}
