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
 * The Original Code is Bug 378079 unit test code.
 *
 * The Initial Developer of the Original Code is POTI Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Crocker <matt@songbirdnest.com>
 *   Seth Spitzer <sspitzer@mozilla.org>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
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

/**
 * Test for bug 401869 to allow multiple words separated by spaces to match in
 * the page title, page url, or bookmark title to be considered a match. All
 * terms must match but not all terms need to be in the title, etc. Also, if
 * any part of the bookmark's title matches, it should be displayed over the
 * page's title.
 */

let current_test = 0;

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

  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },

  onSearchComplete: function() {},

  popupOpen: false,

  popup: {
    setSelectedIndex: function(aIndex) {},
    invalidate: function() {},

    // nsISupports implementation
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsISupports implementation
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

function ensure_results(aSearch, aExpected)
{
  let controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  let input = new AutoCompleteInput(["history"]);

  controller.input = input;

  input.onSearchComplete = function() {
    // If we expect results, make sure we got matches
    do_check_eq(controller.searchStatus, aExpected.length ?
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH :
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);

    // Make sure we have the right number of results
    do_check_eq(controller.matchCount, aExpected.length);

    // Check to see the expected uris and titles match up (in any order)
    for (let i = 0; i < controller.matchCount; i++) {
      let value = controller.getValueAt(i);
      let comment = controller.getCommentAt(i);

      print("Looking for an expected result of " + value + ", " + comment + "...");
      let j;
      for (j = 0; j < aExpected.length; j++) {
        let [uri, title] = aExpected[j];
        // Skip processed expected results
        if (uri == undefined) continue;
        // Load the real uri and titles
        [uri, title] = [kURIs[uri], kTitles[title]];

        // Got a match on both uri and title?
        if (uri == value && title == comment) {
          print("Got it at index " + j + "!!");
          // Make it undefined so we don't process it again
          aExpected[j] = [,];
          break;
        }
      }

      // We didn't hit the break, so we must have not found it
      if (j == aExpected.length)
        do_throw("Didn't find the current result (" + value + ", " + comment + ") in expected: " + aExpected);
    }

    // Fetch the next test if we have more
    if (++current_test < gTests.length)
      run_test();

    do_test_finished();
  };

  let search = kSearches[aSearch];
  print("Searching for.. " + search);
  controller.startSearch(search);
}

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
  var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
} catch(ex) {
  do_throw("Could not get services\n");
} 

// Some date not too long ago
let gDate = new Date(Date.now() - 1000 * 60 * 60) * 1000;

// Define some shared uris and titles
let kURIs = [
  "http://abc/def",
  "http://def/ghi",
  "http://ghi/jkl",
  "http://jkl/mno",
];
let kTitles = [
  "foo bar",
  "bar baz",
];
let kSearches = [
  "c d",
  "b e",
  "b a z",
  "k f t",
  "d i g z",
  "m o z i",
];
  
function addPageBook(aURI, aTitle, aBook)
{
  let uri = iosvc.newURI(kURIs[aURI], null, null);
  let title = kTitles[aTitle];

  print("Adding page/book: " + [aURI, aTitle, aBook, kURIs[aURI], title].join(", "));
  // Add the page and a visit for good measure
  histsvc.setPageDetails(uri, title, 1, false, true);
  histsvc.addVisit(uri, gDate, null, histsvc.TRANSITION_TYPED, false, 0);
  
  // Add a bookmark if we need to
  if (aBook != undefined) {
    let book = kTitles[aBook];
    bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder, uri, bmsvc.DEFAULT_INDEX, book);
  }
}

addPageBook(0, 0);
addPageBook(1, 1);
addPageBook(2, 0, 0);
addPageBook(3, 0, 1);

/**
 * Test history autocomplete
 */
let gTests = [
  ["0: Match 2 terms all in url", [[0,0]]],
  ["1: Match 1 term in url and 1 term in title", [[0,0],[1,1]]],
  ["2: Match 3 terms all in title; display bookmark title if matched", [[1,1],[3,1]]],
  ["3: Match 2 terms in url and 1 in title; bookmark title didn't match", [[2,0],[3,0]]],
  ["4: Match 3 terms in url and 1 in title", [[1,1]]],
  ["5: Match nothing", []],
];

function run_test() {
  print("\n");
  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  // Load the test and print a description then run the test
  let [description, expected] = gTests[current_test];
  print(description);
  ensure_results(current_test, expected);
}
