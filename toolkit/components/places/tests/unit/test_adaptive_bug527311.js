/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Unit Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
