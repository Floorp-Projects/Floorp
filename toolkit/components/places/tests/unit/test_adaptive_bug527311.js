/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL = "http://adapt.mozilla.org/";
const SEARCH_STRING = "adapt";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let ps = Cc["@mozilla.org/preferences-service;1"].
         getService(Ci.nsIPrefBranch);

const PLACES_AUTOCOMPLETE_FEEDBACK_UPDATED_TOPIC =
  "places-autocomplete-feedback-updated";

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

  getSearchAt: function ACI_getSearchAt(aIndex) {
    return this.searches[aIndex];
  },

  onSearchComplete: function ACI_onSearchComplete() {},

  popupOpen: false,

  popup: {
    setSelectedIndex: function() {},
    invalidate: function() {},

    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  onSearchBegin: function() {},

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}


function check_results() {
  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  let input = new AutoCompleteInput(["history"]);
  controller.input = input;

  input.onSearchComplete = function() {
    do_check_eq(controller.searchStatus,
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
    do_check_eq(controller.matchCount, 0);

    remove_all_bookmarks();
    do_test_finished();
 };

  controller.startSearch(SEARCH_STRING);
}


function addAdaptiveFeedback(aUrl, aSearch, aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      os.removeObserver(observer, PLACES_AUTOCOMPLETE_FEEDBACK_UPDATED_TOPIC);
      do_timeout(0, aCallback);
    }
  };
  os.addObserver(observer, PLACES_AUTOCOMPLETE_FEEDBACK_UPDATED_TOPIC, false);

  let thing = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput,
                                           Ci.nsIAutoCompletePopup,
                                           Ci.nsIAutoCompleteController]),
    get popup() { return thing; },
    get controller() { return thing; },
    popupOpen: true,
    selectedIndex: 0,
    getValueAt: function() aUrl,
    searchString: aSearch
  };

  os.notifyObservers(thing, "autocomplete-will-enter-text", null);
}


function run_test() {
  do_test_pending();

  // Add a bookmark to our url.
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri(TEST_URL),                   
                    bs.DEFAULT_INDEX, "test_book");
  // We want to search only history.
  ps.setIntPref("browser.urlbar.default.behavior",
                Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY);
  // Add an adaptive entry.
  addAdaptiveFeedback(TEST_URL, SEARCH_STRING, check_results);
}
