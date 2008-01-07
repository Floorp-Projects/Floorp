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

// Get browser history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get navhistory service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
} 

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

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
  expiredURIs: [],
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
var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var dbFile = dirService.get("ProfD", Ci.nsIFile);
dbFile.append("places.sqlite");

var dbService = Cc["@mozilla.org/storage/service;1"].getService(Ci.mozIStorageService);
var dbConnection = dbService.openDatabase(dbFile);
  

var testURI = uri("http://mozilla.com");
var testAnnoName = "tests/expiration/history";
var testAnnoVal = "foo";
var bookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, bmsvc.DEFAULT_INDEX, "foo");
var triggerURI = uri("http://foobar.com");

// main
function run_test() {
  /*
  test that nsIBrowserHistory.removePagesFromHost does remove expirable annotations
  but doesn't remove bookmarks or EXPIRE_NEVER annotations.
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Hist", testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Never", testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  bhist.removePagesFromHost("mozilla.com", false);
  do_check_eq(bmsvc.getBookmarkURI(bookmark).spec, testURI.spec);
  try {
    annosvc.getPageAnnotation(testAnnoName + "Hist");
    do_throw("nsIBrowserHistory.removePagesFromHost() didn't remove an EXPIRE_WITH_HISTORY annotation");
  } catch(ex) {}
  do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "Never"), testAnnoVal);
  annosvc.removePageAnnotation(testURI, testAnnoName + "Never");

  /*
  test that nsIBrowserHistory.removeAllPages does remove expirable annotations
  but doesn't remove bookmarks or EXPIRE_NEVER annotations.
  */
  var removeAllTestURI = uri("http://removeallpages.com");
  var removeAllTestURINever = uri("http://removeallpagesnever.com");
  histsvc.addVisit(removeAllTestURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  var bmURI = uri("http://bookmarked");
  bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, bmURI, bmsvc.DEFAULT_INDEX, "foo");
  //bhist.addPageWithDetails(placeURI, "place uri", Date.now());
  var placeURI = uri("place:folder=23");
  bhist.addPageWithDetails(placeURI, "place uri", Date.now());
  annosvc.setPageAnnotation(removeAllTestURI, testAnnoName + "Hist", testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  annosvc.setPageAnnotation(removeAllTestURINever, testAnnoName + "Never", testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  bhist.removeAllPages();
  try {
    annosvc.getPageAnnotation(removeAllTestURI, testAnnoName + "Hist");
    do_throw("nsIBrowserHistory.removeAllPages() didn't remove an EXPIRE_WITH_HISTORY annotation");
  } catch(ex) {}
  // test that the moz_places record was removed for this URI
  do_check_eq(histsvc.getPageTitle(removeAllTestURI), null);
  try {
    do_check_eq(annosvc.getPageAnnotation(removeAllTestURINever, testAnnoName + "Never"), testAnnoVal);
    annosvc.removePageAnnotation(removeAllTestURINever, testAnnoName + "Never");
  } catch(ex) {
    do_throw("nsIBrowserHistory.removeAllPages deleted EXPIRE_NEVER annos!");
  }
  // test that the moz_places record was not removed for EXPIRE_NEVER anno
  do_check_neq(histsvc.getPageTitle(removeAllTestURINever), null);
  // for place URI
  do_check_neq(histsvc.getPageTitle(placeURI), null);
  // for bookmarked URI
  do_check_neq(histsvc.getPageTitle(bmURI), null);

  /*
  test anno expiration (expire never)
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  histsvc.removeAllPages();
  // anno should still be there
  do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName), testAnnoVal);
  do_check_eq(annosvc.getItemAnnotation(bookmark, testAnnoName), testAnnoVal);
  annosvc.removeItemAnnotation(bookmark, testAnnoName);

  /*
  test anno expiration (expire with history)
  */
  histsvc.addVisit(testURI, Date.now() * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  histsvc.removeAllPages();
  try {
    annosvc.getPageAnnotation(testURI, testAnnoName);
    do_throw("page still had expire_with_history anno");
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
  // modify it's value
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

  startIncrementalExpirationTests();
}

// incremental expiration tests
// run async, chained

function startIncrementalExpirationTests() {
  startExpireNeither();
}

var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
var ghist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIGlobalHistory2);

/*
test 1: NO EXPIRATION CRITERIA MET

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
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);

  // set visit cap to 2
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
  - add a visit, 4 days old
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 2
  - set browser.history_expire_sites to 2
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm nothing was expired
  - query for the visit, confirm it's there
*/
function startExpireDaysOnly() {
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;

  dump("startExpireDaysOnly()\n");

  // add expirable visit
  histsvc.addVisit(testURI, (Date.now() - (86400 * 2 * 1000)) * 1000, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);

  // add un-expirable visit
  histsvc.addVisit(uri("http://unexpirable.com"), (Date.now() - (86400 * 1000)) * 1000, null, histsvc.TRANSITION_TYPED, false, 0);

  // set visit cap to 2
  prefs.setIntPref("browser.history_expire_sites", 2);
  // set date minimum to 2
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
    do_check_eq(observer.expiredURI, null);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 1);
    // test unexpired record
    do_check_neq(histsvc.getPageTitle(uri("http://unexpirable.com")), null);
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
  - add a visit, 2 days old
  - add a visit, 2 days old
  - set browser.history_expire_days to 3
  - set browser.history_expire_days_min to 1
  - set browser.history_expire_sites to 1
  - kick off incremental expiration

confirmation:
  - check onPageExpired, confirm our visit was expired
  - query for the visit, confirm it's not there
*/
function startExpireBoth() {
  // setup
  histsvc.removeAllPages();
  observer.expiredURI = null;
  dump("starting expiration test 3: both criteria met\n");

  // add visits
  // 2 days old, in microseconds
  var age = (Date.now() - (86400 * 2 * 1000)) * 1000;
  dump("AGE: " + age + "\n");
  histsvc.addVisit(testURI, age, null, histsvc.TRANSITION_TYPED, false, 0);
  histsvc.addVisit(testURI, age, null, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);

  // set visit cap to 1
  prefs.setIntPref("browser.history_expire_sites", 1);
  // set date max to 3
  prefs.setIntPref("browser.history_expire_days", 3);
  // set date minimum to 1
  prefs.setIntPref("browser.history_expire_days_min", 1);

  // trigger expiration
  ghist.addURI(triggerURI, false, true, null);

  // setup confirmation
  do_timeout(3600, "checkExpireBoth();"); // incremental expiration timer is 3500
}

function checkExpireBoth() {
  try {
    do_check_eq(observer.expiredURI, testURI.spec);
    do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 0);
  } catch(ex) {}
  dump("done expiration test 3\n");
  do_test_finished();
}
