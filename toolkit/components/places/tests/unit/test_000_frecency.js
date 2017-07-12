/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
} catch (ex) {
  do_throw("Could not get services\n");
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
  framedLinkVisitBonus: Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK,
  linkVisitBonus: Ci.nsINavHistoryService.TRANSITION_LINK,
  typedVisitBonus: Ci.nsINavHistoryService.TRANSITION_TYPED,
  bookmarkVisitBonus: Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
  downloadVisitBonus: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
  permRedirectVisitBonus: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
  tempRedirectVisitBonus: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
  reloadVisitBonus: Ci.nsINavHistoryService.TRANSITION_RELOAD,
};

// create test data
var searchTerm = "frecency";
var results = [];
var now = Date.now();
var prefPrefix = "places.frecency.";

async function task_initializeBucket(bucket) {
  let [cutoffName, weightName] = bucket;
  // get pref values
  var weight = Services.prefs.getIntPref(prefPrefix + weightName, 0);
  var cutoff = Services.prefs.getIntPref(prefPrefix + cutoffName, 0);
  if (cutoff < 1)
    return;

  // generate a date within the cutoff period
  var dateInPeriod = (now - ((cutoff - 1) * 86400 * 1000)) * 1000;

  for (let [bonusName, visitType] of Object.entries(bonusPrefs)) {
    var frecency = -1;
    var calculatedURI = null;
    var matchTitle = "";
    var bonusValue = Services.prefs.getIntPref(prefPrefix + bonusName);
    // unvisited (only for first cutoff date bucket)
    if (bonusName == "unvisitedBookmarkBonus" || bonusName == "unvisitedTypedBonus") {
      if (cutoffName == "firstBucketCutoff") {
        let points = Math.ceil(bonusValue / parseFloat(100.0) * weight);
        var visitCount = 1; // bonusName == "unvisitedBookmarkBonus" ? 1 : 0;
        frecency = Math.ceil(visitCount * points);
        calculatedURI = uri("http://" + searchTerm + ".com/" +
          bonusName + ":" + bonusValue + "/cutoff:" + cutoff +
          "/weight:" + weight + "/frecency:" + frecency);
        if (bonusName == "unvisitedBookmarkBonus") {
          matchTitle = searchTerm + "UnvisitedBookmark";
          await PlacesUtils.bookmarks.insert({
            parentGuid: PlacesUtils.bookmarks.unfiledGuid,
            url: calculatedURI,
            title: matchTitle
          });
        } else {
          matchTitle = searchTerm + "UnvisitedTyped";
          await PlacesTestUtils.addVisits({
            uri: calculatedURI,
            title: matchTitle,
            transition: visitType,
            visitDate: now
          });
          histsvc.markPageAsTyped(calculatedURI);
        }
      }
    } else {
      // visited
      // visited bookmarks get the visited bookmark bonus twice
      if (visitType == Ci.nsINavHistoryService.TRANSITION_BOOKMARK)
        bonusValue = bonusValue * 2;

      let points = Math.ceil(1 * ((bonusValue / parseFloat(100.000000)).toFixed(6) * weight) / 1);
      if (!points) {
        if (visitType == Ci.nsINavHistoryService.TRANSITION_EMBED ||
            visitType == Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK ||
            visitType == Ci.nsINavHistoryService.TRANSITION_DOWNLOAD ||
            visitType == Ci.nsINavHistoryService.TRANSITION_RELOAD ||
            bonusName == "defaultVisitBonus")
          frecency = 0;
        else
          frecency = -1;
      } else
        frecency = points;
      calculatedURI = uri("http://" + searchTerm + ".com/" +
        bonusName + ":" + bonusValue + "/cutoff:" + cutoff +
        "/weight:" + weight + "/frecency:" + frecency);
      if (visitType == Ci.nsINavHistoryService.TRANSITION_BOOKMARK) {
        matchTitle = searchTerm + "Bookmarked";
        await PlacesUtils.bookmarks.insert({
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          url: calculatedURI,
          title: matchTitle
        });
      } else
        matchTitle = calculatedURI.spec.substr(calculatedURI.spec.lastIndexOf("/") + 1);
      await PlacesTestUtils.addVisits({
        uri: calculatedURI,
        transition: visitType,
        visitDate: dateInPeriod
      });
    }

    if (calculatedURI && frecency) {
      results.push([calculatedURI, frecency, matchTitle]);
      await PlacesTestUtils.addVisits({
        uri: calculatedURI,
        title: matchTitle,
        transition: visitType,
        visitDate: dateInPeriod
      });
    }
  }
}

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
    QueryInterface(iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAutoCompletePopup))
        return this;

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsISupports implementation
  QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAutoCompleteInput))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

add_task(async function test_frecency() {
  // Disable autoFill for this test.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
  do_register_cleanup(() => Services.prefs.clearUserPref("browser.urlbar.autoFill"));
  for (let bucket of bucketPrefs) {
    await task_initializeBucket(bucket);
  }

  // sort results by frecency
  results.sort((a, b) => b[1] - a[1]);
  // Make sure there's enough results returned
  Services.prefs.setIntPref("browser.urlbar.maxRichResults", results.length);

  // DEBUG
  // results.every(function(el) { dump("result: " + el[1] + ": " + el[0].spec + " (" + el[2] + ")\n"); return true; })

  await PlacesTestUtils.promiseAsyncUpdates();

  var controller = Components.classes["@mozilla.org/autocomplete/controller;1"].
                   getService(Components.interfaces.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete
  var input = new AutoCompleteInput(["unifiedcomplete"]);

  controller.input = input;

  // always search in history + bookmarks, no matter what the default is
  Services.prefs.setIntPref("browser.urlbar.search.sources", 3);
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 0);

  var numSearchesStarted = 0;
  input.onSearchBegin = function() {
    numSearchesStarted++;
    do_check_eq(numSearchesStarted, 1);
  };

  await new Promise(resolve => {
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
          let getFrecency = aURL => aURL.match(/frecency:(-?\d+)$/)[1];
          print("### checking for same frecency between '" + searchURL +
                "' and '" + expectURL + "'");
          do_check_eq(getFrecency(searchURL), getFrecency(expectURL));
        }
      }
      resolve();
    };

    controller.startSearch(searchTerm);

  });
});
