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

function run_test() {
  run_next_test();
}

function* cleanup() {
  Services.prefs.clearUserPref("browser.urlbar.autocomplete.enabled");
  Services.prefs.clearUserPref("browser.urlbar.autoFill");
  Services.prefs.clearUserPref("browser.urlbar.autoFill.typed");
  remove_all_bookmarks();
  yield promiseClearHistory();
}
do_register_cleanup(cleanup);

/**
 * @param aSearches
 *        Array of AutoCompleteSearch names.
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  popup: {
    selectedIndex: -1,
    invalidate: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup])
  },
  popupOpen: false,

  disableAutoComplete: false,
  completeDefaultIndex: true,
  completeSelectedIndex: true,
  forceComplete: false,

  minResultsForPopup: 0,
  maxRows: 0,

  showCommentColumn: false,
  showImageColumn: false,

  timeout: 10,
  searchParam: "",

  get searchCount() {
    return this.searches.length;
  },
  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },

  textValue: "",
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

  onSearchBegin: function () {},
  onSearchComplete: function () {},

  onTextEntered: function() false,
  onTextReverted: function() false,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput])
}

function* check_autocomplete(test) {
  // At this point frecency could still be updating due to latest pages
  // updates.
  // This is not a problem in real life, but autocomplete tests should
  // return reliable resultsets, thus we have to wait.
  yield promiseAsyncUpdates();

  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput(["unifiedcomplete"]);
  input.textValue = test.search;

  // Caret must be at the end for autoFill to happen.
  let strLen = test.search.length;
  input.selectTextRange(strLen, strLen);
  Assert.equal(input.selectionStart, strLen, "Selection starts at end");
  Assert.equal(input.selectionEnd, strLen, "Selection ends at the end");

  let controller = Cc["@mozilla.org/autocomplete/controller;1"]
                     .getService(Ci.nsIAutoCompleteController);
  controller.input = input;

  let numSearchesStarted = 0;
  input.onSearchBegin = () => {
    do_log_info("onSearchBegin received");
    numSearchesStarted++;
  };
  let deferred = Promise.defer();
  input.onSearchComplete = () => {
    do_log_info("onSearchComplete received");
    deferred.resolve();
  }

  do_log_info("Searching for: '" + test.search + "'");
  controller.startSearch(test.search);
  yield deferred.promise;

  // We should be running only one query.
  Assert.equal(numSearchesStarted, 1, "Only one search started");

  // Check the autoFilled result.
  Assert.equal(input.textValue, test.autofilled,
               "Autofilled value is correct");

  // Now force completion and check correct casing of the result.
  // This ensures the controller is able to do its magic case-preserving
  // stuff and correct replacement of the user's casing with result's one.
  controller.handleEnter(false);
  Assert.equal(input.textValue, test.completed,
               "Completed value is correct");
}

function addBookmark(aBookmarkObj) {
  Assert.ok(!!aBookmarkObj.url, "Bookmark object contains an url");
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
