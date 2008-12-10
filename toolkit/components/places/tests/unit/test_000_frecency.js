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
 *   Dietrich Ayala <dietrich@mozilla.com>
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

/*

Autocomplete Frecency Tests

- add a visit for each score permutation
- search
- test number of matches
- test each item's location in results

*/

try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var ghist = Cc["@mozilla.org/browser/global-history;2"].
              getService(Ci.nsIGlobalHistory2);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
} catch(ex) {
  do_throw("Could not get services\n");
}

function add_visit(aURI, aVisitDate, aVisitType) {
  var isRedirect = aVisitType == histsvc.TRANSITION_REDIRECT_PERMANENT ||
                   aVisitType == histsvc.TRANSITION_REDIRECT_TEMPORARY;
  var placeID = histsvc.addVisit(aURI, aVisitDate, null,
                                 aVisitType, isRedirect, 0);
  do_check_true(placeID > 0);
  return placeID;
}

var bucketPrefs = [
  [ "firstBucketCutoff", "firstBucketWeight"],
  [ "secondBucketCutoff", "secondBucketWeight"],
  [ "thirdBucketCutoff", "thirdBucketWeight"],
  [ "fourthBucketCutoff", "fourthBucketWeight"],
  [ null, "defaultBucketWeight"]
];

var bonusPrefs = {
  embedVisitBonus: Ci.nsINavHistoryService.TRANSITION_EMBED,
  linkVisitBonus: Ci.nsINavHistoryService.TRANSITION_LINK,
  typedVisitBonus: Ci.nsINavHistoryService.TRANSITION_TYPED,
  bookmarkVisitBonus: Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
  downloadVisitBonus: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
  permRedirectVisitBonus: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
  tempRedirectVisitBonus: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
  defaultVisitBonus: 0,
  unvisitedBookmarkBonus: 0
// XXX todo
// unvisitedTypedBonus: 0
};

// create test data
var searchTerm = "frecency";
var results = [];
var matchCount = 0;
var now = Date.now();
var prefPrefix = "places.frecency.";
bucketPrefs.every(function(bucket) {
  let [cutoffName, weightName] = bucket;
  // get pref values
  var weight = 0, cutoff = 0, bonus = 0;
  try {
    weight = prefs.getIntPref(prefPrefix + weightName);
  } catch(ex) {}
  try {
    cutoff = prefs.getIntPref(prefPrefix + cutoffName);
  } catch(ex) {}

  if (cutoff < 1)
    return true;

  // generate a date within the cutoff period
  var dateInPeriod = (now - ((cutoff - 1) * 86400 * 1000)) * 1000;

  for (let [bonusName, visitType] in Iterator(bonusPrefs)) {
    var frecency = -1;
    var calculatedURI = null;
    var matchTitle = "";
    var bonusValue = prefs.getIntPref(prefPrefix + bonusName);
    // unvisited (only for first cutoff date bucket)
    if (bonusName == "unvisitedBookmarkBonus" || bonusName == "unvisitedTypedBonus") {
      if (cutoffName == "firstBucketCutoff") {
        var points = Math.ceil(bonusValue / parseFloat(100.0) * weight); 
        var visitCount = 1; //bonusName == "unvisitedBookmarkBonus" ? 1 : 0;
        frecency = Math.ceil(visitCount * points);
        calculatedURI = uri("http://" + searchTerm + ".com/" +
          bonusName + ":" + bonusValue + "/cutoff:" + cutoff +
          "/weight:" + weight + "/frecency:" + frecency);
        if (bonusName == "unvisitedBookmarkBonus") {
          matchTitle = searchTerm + "UnvisitedBookmark";
          bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder, calculatedURI, bmsvc.DEFAULT_INDEX, matchTitle);
        }
        else {
          matchTitle = searchTerm + "UnvisitedTyped";
          ghist.setPageTitle(calculatedURI, matchTitle);
          bhist.markPageAsTyped(calculatedURI);
        }
      }
    }
    else {
      // visited
      var points = Math.ceil(1 * ((bonusValue / parseFloat(100.000000)).toFixed(6) * weight) / 1);
      if (!points) {
        if (!visitType ||
            visitType == Ci.nsINavHistoryService.TRANSITION_EMBED ||
            visitType == Ci.nsINavHistoryService.TRANSITION_DOWNLOAD ||
            bonusName == "defaultVisitBonus")
          frecency = 0;
        else
          frecency = -1;
      }
      else
        frecency = points;
      calculatedURI = uri("http://" + searchTerm + ".com/" +
        bonusName + ":" + bonusValue + "/cutoff:" + cutoff +
        "/weight:" + weight + "/frecency:" + frecency);
      if (visitType == Ci.nsINavHistoryService.TRANSITION_BOOKMARK) {
        matchTitle = searchTerm + "Bookmarked";
        bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder, calculatedURI, bmsvc.DEFAULT_INDEX, matchTitle);
      }
      else
        matchTitle = calculatedURI.spec.substr(calculatedURI.spec.lastIndexOf("/")+1);
      add_visit(calculatedURI, dateInPeriod, visitType);
    }

    if (calculatedURI && frecency)
      results.push([calculatedURI, frecency, matchTitle]);
  }
  return true;
});

// sort results by frecency
results.sort(function(a,b) a[1] - b[1]);
results.reverse();
// Make sure there's enough results returned
prefs.setIntPref("browser.urlbar.maxRichResults", results.length);

//results.every(function(el) { dump("result: " + el[1] + ": " + el[0].spec + " (" + el[2] + ")\n"); return true; })

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
  
  onSearchBegin: function() {},
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

function run_test() {
  var controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);  
  
  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["history"]);

  controller.input = input;

  // always search in history + bookmarks, no matter what the default is
  prefs.setIntPref("browser.urlbar.search.sources", 3);

  // Search is asynchronous, so don't let the test finish immediately
  do_test_pending();

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  input.onSearchComplete = function() {
    do_check_eq(numSearchesStarted, 1);
    do_check_eq(controller.searchStatus, 
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH);

    // test that all records with non-zero frecency were matched
    do_check_eq(controller.matchCount, results.length);

    // test that matches are sorted by frecency
    for (var i = 0; i < controller.matchCount; i++) {
      let searchURL = controller.getValueAt(i);
      let expectURL = results[i][0].spec;
      if (searchURL == expectURL) {
        do_check_eq(controller.getValueAt(i), results[i][0].spec);
        do_check_eq(controller.getCommentAt(i), results[i][2]);
      } else {
        // If the results didn't match exactly, perhaps it's still the right
        // frecency just in the wrong "order" (order of same frecency is
        // undefined), so check if frecency matches. This is okay because we
        // can still ensure the correct number of expected frecencies.
        let getFrecency = function(aURL) aURL.match(/frecency:(-?\d+)$/)[1];
        do_check_eq(getFrecency(searchURL), getFrecency(expectURL));
      }
    }

    do_test_finished();
  };

  controller.startSearch(searchTerm);
}
