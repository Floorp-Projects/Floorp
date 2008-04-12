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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * Test bug 418257 by making sure tags are returned with the title as part of
 * the "comment" if there are tags even if we didn't match in the tags. They
 * are separated from the title by a endash.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
let current_test = 0;

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  timeout: 10,
  textValue: "",
  searches: null,
  searchParam: "",
  popupOpen: false,
  minResultsForPopup: 0,
  invalidate: function() {},
  disableAutoComplete: false,
  completeDefaultIndex: false,
  get popup() { return this; },
  onSearchBegin: function() {},
  onSearchComplete: function() {},
  setSelectedIndex: function() {},
  get searchCount() { return this.searches.length; },
  getSearchAt: function(aIndex) this.searches[aIndex],
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput, Ci.nsIAutoCompletePopup])
};

function ensure_results(aSearch, aExpected)
{
  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  let input = new AutoCompleteInput(["history"]);

  controller.input = input;

  let numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    do_check_eq(numSearchesStarted, 1);
    aExpected = aExpected.slice();

    // Check to see the expected uris and titles match up (in any order)
    for (let i = 0; i < controller.matchCount; i++) {
      let value = controller.getValueAt(i);
      let comment = controller.getCommentAt(i);

      print("Looking for an expected result of " + value + ", " + comment + "...");
      let j;
      for (j = 0; j < aExpected.length; j++) {
        let [uri, title, tags] = gPages[aExpected[j]];

        // Skip processed expected results
        if (uri == undefined) continue;

        // Load the real uri and titles and tags if necessary
        uri = iosvc.newURI(kURIs[uri], null, null).spec;
        title = kTitles[title];
        if (tags)
          title += " \u2013 " + tags.map(function(aTag) kTitles[aTag]);

        // Got a match on both uri and title?
        if (uri == value && title == comment) {
          print("Got it at index " + j + "!!");
          // Make it undefined so we don't process it again
          aExpected[j] = [];
          break;
        }
      }

      // We didn't hit the break, so we must have not found it
      if (j == aExpected.length)
        do_throw("Didn't find the current result (" + value + ", " + comment + ") in expected: " + aExpected);
    }

    // Make sure we have the right number of results
    do_check_eq(controller.matchCount, aExpected.length);

    // If we expect results, make sure we got matches
    do_check_eq(controller.searchStatus, aExpected.length ?
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH :
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);

    // Fetch the next test if we have more
    if (++current_test < gTests.length)
      run_test();

    do_test_finished();
  };

  print("Searching for.. " + aSearch);
  controller.startSearch(aSearch);
}

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var tagsvc = Cc["@mozilla.org/browser/tagging-service;1"].
               getService(Ci.nsITaggingService);
  var iosvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
} catch(ex) {
  do_throw("Could not get services\n");
}

// Some date not too long ago
let gDate = new Date(Date.now() - 1000 * 60 * 60) * 1000;
// Store the page info for each uri
let gPages = [];

function addPageBook(aURI, aTitle, aBook, aTags, aKey)
{
  // Add a page entry for the current uri
  gPages[aURI] = [aURI, aTitle, aTags];

  let uri = iosvc.newURI(kURIs[aURI], null, null);
  let title = kTitles[aTitle];

  let out = [aURI, aTitle, aBook, aTags, aKey];
  out.push("\nuri=" + kURIs[aURI]);
  out.push("\ntitle=" + title);

  // Add the page and a visit for good measure
  bhist.addPageWithDetails(uri, title, gDate);

  // Add a bookmark if we need to
  if (aBook != undefined) {
    let book = kTitles[aBook];
    let bmid = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder, uri,
      bmsvc.DEFAULT_INDEX, book);
    out.push("\nbook=" + book);

    // Add a keyword to the bookmark if we need to
    if (aKey != undefined)
      bmsvc.setKeywordForBookmark(bmid, aKey);

    // Add tags if we need to
    if (aTags != undefined && aTags.length > 0) {
      // Convert each tag index into the title
      let tags = aTags.map(function(aTag) kTitles[aTag]);
      tagsvc.tagURI(uri, tags);
      out.push("\ntags=" + tags);
    }
  }

  print("\nAdding page/book/tag: " + out.join(", "));
}

function run_test() {
  print("\n");
  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  // Load the test and print a description then run the test
  let [description, search, expected, func] = gTests[current_test];
  print(description);

  // Do an extra function if necessary
  if (func)
    func();

  ensure_results(search, expected);
}

// *************************************************
// *** vvv Custom Test Stuff Goes Below Here vvv ***
// *************************************************

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://page1",
  "http://page2",
  "http://page3",
  "http://page4",
];
let kTitles = [
  "tag1",
  "tag2",
  "tag3",
];

// Add pages with varying number of tags
addPageBook(0, 0, 0, [0]);
addPageBook(1, 0, 0, [0,1]);
addPageBook(2, 0, 0, [0,2]);
addPageBook(3, 0, 0, [0,1,2]);

// For each test, provide a title, the search terms, and an array of uri
// indices of the pages that should be returned, followed by an optional
// function. The uris can be in any order, but must be an index created by
// addPageBook or placed manually into gPages.
let gTests = [
  ["0: Make sure tags come back in the title when matching tags",
   "page1 tag", [0]],
  ["1: Check tags in title for page2",
   "page2 tag", [1]],
  ["2: Make sure tags appear even when not matching the tag",
   "page3", [2]],
  ["3: Multiple tags come in commas for page4",
   "page4", [3]],
  ["4: Extra test just to make sure we match the title",
   "tag2", [1,3]],
];
