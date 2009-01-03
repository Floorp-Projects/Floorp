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
 * Test for bug 395739 to make sure the feedback to the search results in those
 * entries getting better ranks. Additionally, exact matches should be ranked
 * higher. Because the interactions among adaptive rank and visit counts is not
 * well defined, this test holds one of the two values constant when modifying
 * the other.
 *
 * This also tests bug 395735 for the instrumentation feedback mechanism.
 *
 * Bug 411293 is tested to make sure the drop down strongly prefers previously
 * typed pages that have been selected and are moved to the top with adaptive
 * learning.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Wait for a maximum of MAX_POLLING_TIMEOUT milliseconds before timing out
// while polling database.
// Since feedback increase is done async we must wait that the async statement
// gets executed, so we have to poll the database until data is consistent.
const MAX_POLLING_TIMEOUT = 3000;

// Get services
let histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
let db = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsPIPlacesDatabase).
         DBConnection;
let bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
let obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);

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

/**
 * Checks that autocomplete results are ordered correctly
 */
function ensure_results(uris, searchTerm)
{
  let controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  let input = new AutoCompleteInput(["history"]);

  controller.input = input;

  input.onSearchComplete = function() {
    do_check_eq(controller.searchStatus,
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);
    do_check_eq(controller.matchCount, uris.length);
    for (let i = 0; i < controller.matchCount; i++) {
      do_check_eq(controller.getValueAt(i), uris[i].spec);
    }

    // start the next test or finish testing if no more tests are available
    if (current_test < (tests.length - 1)) {
      current_test++;
      tests[current_test]();
    }
    else
      do_test_finished();
  };

  controller.startSearch(searchTerm);
}

/**
 * Bump up the rank for an uri, returning the expected use_count value, we will
 * use this value to poll database.
 */
function setCountRank(aURI, aCount, aRank, aSearch)
{
  // Bump up the visit count for the uri
  for (let i = 0; i < aCount; i++)
    histsvc.addVisit(aURI, d1, null, histsvc.TRANSITION_TYPED, false, 0);

  // Make a nsIAutoCompleteController and friends for instrumentation feedback
  let thing = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput,
                                           Ci.nsIAutoCompletePopup,
                                           Ci.nsIAutoCompleteController]),
    get popup() { return thing; },
    get controller() { return thing; },
    popupOpen: true,
    selectedIndex: 0,
    getValueAt: function() aURI.spec,
    searchString: aSearch
  };


  // Bump up the instrumentation feedback, calculating the target use_count
  // we will reach after all updates
  let targetUseCount = 0;
  for (let i = 0; i < aRank; i++) {
    obs.notifyObservers(thing, "autocomplete-will-enter-text", null);
    targetUseCount = (targetUseCount * 0.9) + 1;
  }

  return Math.floor(targetUseCount);
}

let uri1 = uri("http://site.tld/1");
let uri2 = uri("http://site.tld/2");

// d1 is some date for the page visit
let d1 = new Date(Date.now() - 1000 * 60 * 60) * 1000;
// c1 is larger (should show up higher) than c2
let c1 = 10;
let c2 = 1;
// s1 is a partial match of s2
let s0 = "";
let s1 = "si";
let s2 = "site";

let curTestItem1 = null;
let curTestItem2 = null;

let pollTime = Date.now();
let stmt = db.createStatement(
    "SELECT use_count " +
    "FROM moz_inputhistory " +
    "WHERE input = ?1 AND place_id = " +
    "(SELECT IFNULL((SELECT id FROM moz_places_temp WHERE url = ?2), " +
                   "(SELECT id FROM moz_places WHERE url = ?2)))");

/**
 * Feedback increase is done async, this function ensures that all data have
 * been written before checking the results.
 */
function poll_database(aSearch) {
  if (Date.now() - pollTime > MAX_POLLING_TIMEOUT)
    do_throw("*** TIMEOUT ***: The test timed out while polling database.\n");
  stmt.bindUTF8StringParameter(0, curTestItem1.input);
  stmt.bindUTF8StringParameter(1, curTestItem1.uri.spec);
  if (!stmt.executeStep()) {
    stmt.reset();
    do_timeout(100, "poll_database('" + aSearch + "');");
    return;
  }
  let useCount1 = stmt.getInt64(0);
  stmt.reset();
  stmt.bindUTF8StringParameter(0, curTestItem2.input);
  stmt.bindUTF8StringParameter(1, curTestItem2.uri.spec);
  if (!stmt.executeStep()) {
    stmt.reset();
    do_timeout(100, "poll_database('" + aSearch + "');");
    return;
  }
  let useCount2 = stmt.getInt64(0);
  stmt.reset();
  if (useCount1 == curTestItem1.useCount && useCount2 == curTestItem2.useCount) {
    // All data has been written, so we can check results
    ensure_results([curTestItem1.uri, curTestItem2.uri], aSearch);
  }
  else {
    // Some data is missing, poll again after 100ms
    do_timeout(100, "poll_database('" + aSearch + "');");
  }
}

/**
 * Clean up database for next test
 */
function prepTest(name) {
  print("Test " + name);
  pollTime = Date.now();
  bhist.removeAllPages();
}

let current_test = 0;

let tests = [
// Test things without a search term
function() {
  prepTest("0 same count, diff rank, same term; no search");
  let uc1 = setCountRank(uri1, c1, c1, s2);
  let uc2 = setCountRank(uri2, c1, c2, s2);
  curTestItem1 = { uri: uri1, input: s2, useCount: uc1 };
  curTestItem2 = { uri: uri2, input: s2, useCount: uc2 };
  do_timeout(100, "poll_database('" + s0 + "');");
},
function() {
  prepTest("1 same count, diff rank, same term; no search");
  let uc1 = setCountRank(uri1, c1, c2, s2);
  let uc2 = setCountRank(uri2, c1, c1, s2);
  curTestItem2 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem1 = {uri: uri2, input: s2, useCount: uc2};
  do_timeout(100, "poll_database('" + s0 + "');");
},
function() {
  prepTest("2 diff count, same rank, same term; no search");
  let uc1 = setCountRank(uri1, c1, c1, s2);
  let uc2 = setCountRank(uri2, c2, c1, s2);
  curTestItem1 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem2 = {uri: uri2, input: s2, useCount: uc2};
  do_timeout(100, "poll_database('" + s0 + "');");
},
function() {
  prepTest("3 diff count, same rank, same term; no search");
  let uc1 = setCountRank(uri1, c2, c1, s2);
  let uc2 = setCountRank(uri2, c1, c1, s2);
  curTestItem2 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem1 = {uri: uri2, input: s2, useCount: uc2};  
  do_timeout(100, "poll_database('" + s0 + "');");
},

// Test things with a search term (exact match one, partial other)
function() {
  prepTest("4 same count, same rank, diff term; one exact/one partial search");
  let uc1 = setCountRank(uri1, c1, c1, s1);
  let uc2 = setCountRank(uri2, c1, c1, s2);
  curTestItem1 = {uri: uri1, input: s1, useCount: uc1};
  curTestItem2 = {uri: uri2, input: s2, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
},
function() {
  prepTest("5 same count, same rank, diff term; one exact/one partial search");
  let uc1 = setCountRank(uri1, c1, c1, s2);
  let uc2 = setCountRank(uri2, c1, c1, s1);
  curTestItem2 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem1 = {uri: uri2, input: s1, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
},

// Test things with a search term (exact match both)
function() {
  prepTest("6 same count, diff rank, same term; both exact search");
  let uc1 = setCountRank(uri1, c1, c1, s1);
  let uc2 = setCountRank(uri2, c1, c2, s1);
  curTestItem1 = {uri: uri1, input: s1, useCount: uc1};
  curTestItem2 = {uri: uri2, input: s1, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
},
function() {
  prepTest("7 same count, diff rank, same term; both exact search");
  let uc1 = setCountRank(uri1, c1, c2, s1);
  let uc2 = setCountRank(uri2, c1, c1, s1);
  curTestItem2 = {uri: uri1, input: s1, useCount: uc1};
  curTestItem1 = {uri: uri2, input: s1, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
},

// Test things with a search term (partial match both)
function() {
  prepTest("8 same count, diff rank, same term; both partial search");
  let uc1 = setCountRank(uri1, c1, c1, s2);
  let uc2 = setCountRank(uri2, c1, c2, s2);
  curTestItem1 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem2 = {uri: uri2, input: s2, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
},
function() {
  prepTest("9 same count, diff rank, same term; both partial search");
  let uc1 = setCountRank(uri1, c1, c2, s2);
  let uc2 = setCountRank(uri2, c1, c1, s2);
  curTestItem2 = {uri: uri1, input: s2, useCount: uc1};
  curTestItem1 = {uri: uri2, input: s2, useCount: uc2};
  do_timeout(100, "poll_database('" + s1 + "');");
}
];

/**
 * Test adapative autocomplete
 */
function run_test() {
  do_test_pending();
  tests[current_test]();
}
