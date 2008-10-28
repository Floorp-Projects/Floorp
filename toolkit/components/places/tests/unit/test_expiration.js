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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Dan Mills <thunder@mozilla.com>
 *  Marco Bonardo <mak77@supereva.it>
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

// execute this test while syncing, this will potentially show possible problems
start_sync();

// Get services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
var ghist = Cc["@mozilla.org/browser/global-history;2"].
            getService(Ci.nsIGlobalHistory2);
var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
              getService(Ci.nsIAnnotationService);
var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

// create and add history observer
var observer = {
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
  },
  onTitleChanged: function(aURI, aPageTitle) {
  },
  onDeleteURI: function(aURI) {
  },
  onClearHistory: function() {
    this.historyCleared = true;
  },
  onPageChanged: function(aURI, aWhat, aValue) {
  },
  expiredURI: null,
  onPageExpired: function(aURI, aVisitTime, aWholeEntry) {
    this.expiredURI = aURI.spec;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
histsvc.addObserver(observer, false);

// get direct db connection for date-based anno tests
var dbConnection = histsvc.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;


var testURI = uri("http://mozilla.com");
var testAnnoName = "tests/expiration/history";
var testAnnoVal = "foo";
var bookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, bmsvc.DEFAULT_INDEX, "foo");
var triggerURI = uri("http://foobar.com");

// main
function run_test() {
  /*
  Test that nsIBrowserHistory.removePagesFromHost removes expirable
  annotations but doesn't remove bookmarks.
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Hist", testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Never", testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  bhist.removePagesFromHost("mozilla.com", false);

  do_check_eq(bmsvc.getBookmarkURI(bookmark).spec, testURI.spec);
  // EXPIRE_WITH_HISTORY anno should be removed since we don't have visits
  try {
    annosvc.getPageAnnotation(testAnnoName + "Hist");
    do_throw("removePagesFromHost() didn't remove an EXPIRE_WITH_HISTORY annotation");
  } catch(ex) {}
  // EXPIRE_NEVER anno should be retained since the uri is bookmarked
  do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "Never"), testAnnoVal);
  // check that moz_places record was not removed for this URI (is bookmarked)
  do_check_eq(histsvc.getPageTitle(testURI), "mozilla.com");

  //cleanup
  annosvc.removePageAnnotation(testURI, testAnnoName + "Never");

  /*
  Test that nsIBrowserHistory.removeAllPages removes expirable
  annotations but doesn't remove bookmarks.
  */
  var removeAllTestURI = uri("http://removeallpages.com");
  var removeAllTestURINever = uri("http://removeallpagesnever.com");
  histsvc.addVisit(removeAllTestURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  var bmURI = uri("http://bookmarked");
  var bookmark2 = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, bmURI, bmsvc.DEFAULT_INDEX, "foo");
  var placeURI = uri("place:folder=23");
  bhist.addPageWithDetails(placeURI, "place uri", Date.now() * 1000);
  annosvc.setPageAnnotation(removeAllTestURI, testAnnoName + "Hist", testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  annosvc.setPageAnnotation(removeAllTestURINever, testAnnoName + "Never", testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  bhist.removeAllPages();

  // both annotations should be removed since those URIs are not bookmarked
  try {
    annosvc.getPageAnnotation(removeAllTestURI, testAnnoName + "Hist");
    do_throw("nsIBrowserHistory.removeAllPages() didn't remove an EXPIRE_WITH_HISTORY annotation");
  } catch(ex) {}
  try {
    annosvc.getPageAnnotation(removeAllTestURINever, testAnnoName + "Never");
    do_throw("nsIBrowserHistory.removePagesFromHost() didn't remove an EXPIRE_NEVER annotation");
  } catch(ex) {}
  // test that the moz_places record was not removed for place URI
  do_check_neq(histsvc.getPageTitle(placeURI), null);
  // test that the moz_places record was not removed for bookmarked URI
  do_check_neq(histsvc.getPageTitle(bmURI), null);

  // cleanup
  bmsvc.removeItem(bookmark2);

  /*
  Test anno expiration (EXPIRE_NEVER)
    - A page annotation should be expired only if the page is removed from the
      database, i.e. when it has no visits and is not bookmarked
    - An item annotation does not never expire
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  // add page/item annotations to bookmarked uri
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  // add page/item annotations to a not bookmarked uri
  var expireNeverURI = uri("http://expiremenever.com");
  histsvc.addVisit(expireNeverURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(expireNeverURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  histsvc.removeAllPages();  

  // check that page and item annotations are still there for bookmarked uri
  do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName), testAnnoVal);
  do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName), testAnnoVal);
  // check that page annotation has been removed for not bookmarked uri
    try {
    annosvc.getPageAnnotation(expireNeverURI, testAnnoName);
    do_throw("nsIBrowserHistory.removeAllPages() didn't remove an EXPIRE_NEVER annotation");
  } catch(ex) {}

  // do some cleanup
  annosvc.removeItemAnnotation(bookmark, testAnnoName);
  annosvc.removePageAnnotation(testURI, testAnnoName);

  /*
  Test anno expiration (EXPIRE_WITH_HISTORY)
    - A page annotation should be expired when the page has no more visits
      whatever it is bookmarked or not
    - An item annotation cannot have this kind of expiration
  */

  // Add page anno on a bookmarked URI
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  // Check that we can't add an EXPIRE_WITH_HISTORY anno to a bookmark
  try {
    annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
    do_throw("I was able to set an EXPIRE_WITH_HISTORY anno on a bookmark");
  } catch(ex) {}

  histsvc.removeAllPages();

  // check that anno has been expired correctly even if the URI is bookmarked
  try {
    annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had expire_with_history page anno");
  } catch(ex) {}

  /*
  test anno expiration (days)
    - add a bookmark, set an anno on the bookmarked uri 
    - manually tweak moz_annos.dateAdded, making it expired
    - add another entry (to kick off expiration)
    - try to get the anno (should fail. maybe race here? is there a way to determine
      if the page has been added, so we know that expiration is done?)
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);

  // these annotations should be removed (after manually tweaking their dateAdded)
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);

  // set dateAdded to 8 days ago
  var expirationDate = (Date.now() - (8 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // these annotations should remain
  annosvc.setPageAnnotation(testURI, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_DAYS);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 7 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName + "NotExpired");
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 7 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName + "NotExpired");

  // test for expired annos
  try {
    annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had days anno");
  } catch(ex) {}
  try {
    annosvc.getItemAnnotation(bookmark, testAnnoName);
    do_throw("bookmark still had days anno");
  } catch(ex) {}

  // test anno expiration (days) removes annos annos 6 days old
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  // these annotations should remain as they are only 6 days old
  var expirationDate = (Date.now() - (6 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 7 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName);
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 7 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName);


  // test anno expiration (weeks) removes annos 31 days old
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  // these annotations should not remain as they are 31 days old
  var expirationDate = (Date.now() - (31 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);
  // these annotations should remain
  annosvc.setPageAnnotation(testURI, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  annosvc.setItemAnnotation(bookmark, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_WEEKS);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 30 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName + "NotExpired");
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 30 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName + "NotExpired");

  try {
    annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had weeks anno");
  } catch(ex) {}
  try {
    annosvc.getItemAnnotation(bookmark, testAnnoName);
    do_throw("bookmark still had weeks anno");
  } catch(ex) {}

  // test anno expiration (weeks) does not remove annos 29 days old
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  // these annotations should remain as they are only 29 days old
  var expirationDate = (Date.now() - (29 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 30 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName);
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 30 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName);

  // test anno expiration (months) removes annos 181 days old
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  var expirationDate = (Date.now() - (181 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);
  // these annotations should remain
  annosvc.setPageAnnotation(testURI, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  annosvc.setItemAnnotation(bookmark, testAnnoName + "NotExpired", testAnnoVal, 0, annosvc.EXPIRE_MONTHS);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 180 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName + "NotExpired");
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName + "NotExpired"), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 180 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName + "NotExpired");

  try {
    var annoVal = annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had months anno");
  } catch(ex) {}
  try {
    annosvc.getItemAnnotation(bookmark, testAnnoName);
    do_throw("bookmark still had months anno");
  } catch(ex) {}

  // test anno expiration (months) does not remove annos 179 days old
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  // these annotations should remain as they are only 179 days old
  var expirationDate = (Date.now() - (179 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);

  // test for unexpired annos
  try {
    do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("anno < 180 days old was expired!");
  }
  annosvc.removePageAnnotation(testURI, testAnnoName);
  try {
    do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName), testAnnoVal);
  } catch(ex) {
    do_throw("item anno < 180 days old was expired!");
  }
  annosvc.removeItemAnnotation(bookmark, testAnnoName);

  // test anno expiration (session)
  // XXX requires app restart

  // test expiration of anno dateAdded vs lastModified (bug #389902)
  // add an anno w/ days expiration
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  // make it 8 days old
  var expirationDate = (Date.now() - (8 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);
  // modify its value
  annosvc.setPageAnnotation(testURI, testAnnoName, "mod", 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, "mod", 0, annosvc.EXPIRE_DAYS);
  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);
  // anno should still be there
  try {
    var annoVal = annosvc.getPageAnnotation(testURI, testAnnoName);
  } catch(ex) {
    do_throw("page lost a days anno that was 8 days old, but was modified today");
  }
  try {
    annosvc.getItemAnnotation(bookmark, testAnnoName);
  } catch(ex) {
    do_throw("bookmark lost a days anno that was 8 days old, but was modified today");
  }

  // now make the anno lastModified 8 days old
  var expirationDate = (Date.now() - (8 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET lastModified = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET lastModified = " + expirationDate);
  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  bhist.removePage(triggerURI);
  // anno should have been deleted
  try {
    var annoVal = annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had a days anno that was modified 8 days ago");
  } catch(ex) {}
  try {
    annosvc.getItemAnnotation(bookmark, testAnnoName);
    do_throw("bookmark lost a days anno that was modified 8 days ago");
  } catch(ex) {}

  // cleanup
  bmsvc.removeItem(bookmark);
  annosvc.removePageAnnotations(testURI);
  annosvc.removePageAnnotations(triggerURI);

  startIncrementalExpirationTests();
}

// incremental expiration tests
// run async, chained

function startIncrementalExpirationTests() {
  startExpireNeither();
}

/*
test 1: NO EXPIRATION CRITERIA MET (INSIDE SITES CAP)

1. zero visits > {browser.history_expire_days}
2. zero visits > {browser.history_expire_days_min}
   AND total visited site count < {browser.history_expire_sites}

steps:
  - clear history
  - reset observer
  - add a visit, w/ current date
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 2
  - set browser.history_expire_sites to 2
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm nothing was expired
  - query for the visit, confirm it's there

*/
function startExpireNeither() {
  dump("startExpireNeither()\n");
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  // add data
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set sites cap to 2
  prefs.setIntPref("browser.history_expire_sites", 2);
  // set date minimum to 2
  prefs.setIntPref("browser.history_expire_days_min", 2);
  // set date maximum to 3
  prefs.setIntPref("browser.history_expire_days", 3);

  // trigger expiration
  ghist.addURI(triggerURI, false, true, null); 

  // setup confirmation
  do_test_pending();
  do_timeout(3600, "checkExpireNeither();"); // incremental expiration timer is 3500
}

function checkExpireNeither() {
  dump("checkExpireNeither()\n");
  try {
    do_check_eq(observer.expiredURI, null);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 1);
  } catch(ex) {
    do_throw(ex);
  }
  dump("done incremental expiration test 1\n");
  startExpireDaysOnly();
}

/*
test 2: MAX-AGE DATE CRITERIA MET

1. some visits > {browser.history_expire_days}
2. total visited sites count < {browser.history_expire_sites}

steps:
  - clear history
  - reset observer
  - add a visit, 4 days old (expirable)
  - add a visit, 1 day old (not expirable)
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 2
  - set browser.history_expire_sites to 20
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm that the expirable uri was expired
  - query for the visit, confirm it's there
*/
function startExpireDaysOnly() {
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  dump("startExpireDaysOnly()\n");

  // add expirable visit
  histsvc.addVisit(testURI, (Date.now() - (86400 * 4 * 1000)) * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // add un-expirable visit
  var unexpirableURI = uri("http://unexpirable.com");
  histsvc.addVisit(unexpirableURI, (Date.now() - (86400 * 1000)) * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(unexpirableURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set sites cap to 20 (make sure it's not an expiration criteria)
  prefs.setIntPref("browser.history_expire_sites", 20);
  // set date minimum to 2 (also not an expiration criteria)
  prefs.setIntPref("browser.history_expire_days_min", 2);
  // set date maximum to 3
  prefs.setIntPref("browser.history_expire_days", 3);

  // trigger expiration
  ghist.addURI(triggerURI, false, true, null); 

  // setup confirmation
  do_timeout(3600, "checkExpireDaysOnly();"); // incremental expiration timer is 3500
}

function checkExpireDaysOnly() {
  try {
    // test expired record
    do_check_eq(observer.expiredURI, testURI.spec);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 0);

    // test unexpired record
    do_check_neq(histsvc.getPageTitle(unexpirableURI), null);
    do_check_eq(annosvc.getPageAnnotationNames(unexpirableURI, {}).length, 1);
  } catch(ex) {}
  dump("done expiration test 2\n");
  startExpireBoth();
}

/*
test 3: MIN-AGE+VISIT-CAP CRITERIA MET

1. zero visits > {browser.history_expire_days}
2. some visits > {browser.history_expire_days_min}
   AND total visited sites count > {browser.history_expire_sites}

steps:
  - clear history
  - reset observer
  - add a visit to an url, 2 days old
  - add a visit to another url, today
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 1
  - set browser.history_expire_sites to 1
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm our oldest visit was expired
  - query for the oldest visit, confirm it's not there
*/
function startExpireBoth() {
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;
  dump("starting expiration test 3: both criteria met\n");
  // force a sync, this will ensure that later we will have the same place in
  // both temp and disk table, and that the expire site cap count is correct.
  bmsvc.changeBookmarkURI(bookmark, testURI);
  // add visits
  // 2 days old, in microseconds
  var age = (Date.now() - (86400 * 2 * 1000)) * 1000;
  dump("AGE: " + age + "\n");
  histsvc.addVisit(testURI, age, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set sites cap to 1
  prefs.setIntPref("browser.history_expire_sites", 1);
  // set date max to 3
  prefs.setIntPref("browser.history_expire_days", 3);
  // set date minimum to 1
  prefs.setIntPref("browser.history_expire_days_min", 1);

  // trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(triggerURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // setup confirmation
  do_timeout(3600, "checkExpireBoth();"); // incremental expiration timer is 3500
}

function checkExpireBoth() {
  try {
    do_check_eq(observer.expiredURI, testURI.spec);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 0);
    do_check_eq(annosvc.getPageAnnotationNames(triggerURI, {}).length, 1);
  } catch(ex) {}
  dump("done expiration test 3\n");
  startExpireNeitherOver()
}

/*
test 4: NO EXPIRATION CRITERIA MET (OVER SITES CAP)

1. zero visits > {browser.history_expire_days}
2. zero visits > {browser.history_expire_days_min}
   AND total visited site count > {browser.history_expire_sites}

steps:
  - clear history
  - reset observer
  - add a visit to an url, w/ current date
  - add a visit to another url, w/ current date
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 2
  - set browser.history_expire_sites to 1
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm nothing was expired
  - query for the visit, confirm it's there

*/
function startExpireNeitherOver() {
  dump("startExpireNeitherOver()\n");
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  // add data
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set sites cap to 1
  prefs.setIntPref("browser.history_expire_sites", 1);
  // set date minimum to 2
  prefs.setIntPref("browser.history_expire_days_min", 2);
  // set date maximum to 3
  prefs.setIntPref("browser.history_expire_days", 3);

  // trigger expiration
  histsvc.addVisit(triggerURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(triggerURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // setup confirmation
  do_timeout(3600, "checkExpireNeitherOver();"); // incremental expiration timer is 3500
}

function checkExpireNeitherOver() {
  dump("checkExpireNeitherOver()\n");
  try {
    do_check_eq(observer.expiredURI, null);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 1);
    do_check_eq(annosvc.getPageAnnotationNames(triggerURI, {}).length, 1);
  } catch(ex) {
    do_throw(ex);
  }
  dump("done incremental expiration test 4\n");
  startExpireHistoryDisabled();
}

/*
test 5: HISTORY DISABLED (HISTORY_EXPIRE_DAYS = 0), EXPIRE EVERYTHING

1. special case when history is disabled, expire all visits

steps:
  - clear history
  - reset observer
  - add a visit to an url, w/ current date
  - set browser.history_expire_days to 0
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm visit was expired
  - query for the visit, confirm it's not there

*/
function startExpireHistoryDisabled() {
  dump("startExpireHistoryDisabled()\n");
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  // add data
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set date maximum to 0
  prefs.setIntPref("browser.history_expire_days", 0);

  // setup confirmation
  do_timeout(3600, "checkExpireHistoryDisabled();"); // incremental expiration timer is 3500
}

function checkExpireHistoryDisabled() {
  dump("checkExpireHistoryDisabled()\n");
  try {
    do_check_eq(observer.expiredURI, testURI.spec);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 0);
  } catch(ex) {
    do_throw(ex);
  }
  dump("done incremental expiration test 5\n");
  startExpireBadPrefs();
}

/*
test 6: BAD EXPIRATION PREFS (MAX < MIN)

1. if max < min we force max = min to avoid deleting wrong visits

steps:
  - clear history
  - reset observer
  - add a visit to an url, 10 days ago
  - set browser.history_expire_days to 1
  - set browser.history_expire_days_min to 20
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm nothing was expired
  - query for the visit, confirm it's there

*/
function startExpireBadPrefs() {
  dump("startExpireBadPrefs()\n");
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  // add data
  var age = (Date.now() - (86400 * 10 * 1000)) * 1000;
  histsvc.addVisit(testURI, age, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);

  // set date minimum to 20
  prefs.setIntPref("browser.history_expire_days_min", 20);
  // set date maximum to 1
  prefs.setIntPref("browser.history_expire_days", 1);

  // setup confirmation
  do_timeout(3600, "checkExpireBadPrefs();"); // incremental expiration timer is 3500
}

function checkExpireBadPrefs() {
  dump("checkExpireBadPrefs()\n");
  try {
    do_check_eq(observer.expiredURI, null);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 1);
  } catch(ex) {
    do_throw(ex);
  }
  dump("done incremental expiration test 6\n");
  finish_test();
}
