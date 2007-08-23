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

// main
function run_test() {
  var testURI = uri("http://mozilla.com");
  var testAnnoName = "tests/expiration/history";
  var testAnnoVal = "foo";
  var bookmark = bmsvc.insertBookmark(bmsvc.bookmarksRoot, testURI, bmsvc.DEFAULT_INDEX, "foo");
  var triggerURI = uri("http://foobar.com");

  /*
  test that nsIBrowserHistory.removePagesFromHost does remove expirable annotations
  but doesn't remove bookmarks or EXPIRE_NEVER annotations.
  */
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Hist", testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  annosvc.setPageAnnotation(testURI, testAnnoName + "Never", testAnnoVal, 0, annosvc.EXPIRE_NEVER);
  bhist.removeAllPages();
  try {
    annosvc.getPageAnnotation(testAnnoName + "Hist");
    do_throw("nsIBrowserHistory.removePagesFromHost() didn't remove an EXPIRE_WITH_HISTORY annotation");
  } catch(ex) {}
  do_check_eq(annosvc.getPageAnnotation(testURI, testAnnoName + "Never"), testAnnoVal);
  annosvc.removePageAnnotation(testURI, testAnnoName + "Never");

  /*
  test age-based history and anno expiration via the browser.history_expire_days pref.
  steps:
    - add a visit (at least 2 days old)
    - set an anno on that entry
    - set pref to 1 day
    - call histsvc.removeAllPages()
    - check onPageExpired for that entry
  */
  histsvc.addVisit(testURI, Date.now() - (86400 * 2), 0, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WITH_HISTORY);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setIntPref("browser.history_expire_days", 1);
  histsvc.removeAllPages();
  do_check_true(observer.historyCleared);
  do_check_eq(testURI.spec, observer.expiredURI);
  do_check_eq(annosvc.getPageAnnotationNames(testURI, {}).length, 0);

  // get direct db connection for date-based anno tests
  var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
  var dbFile = dirService.get("ProfD", Ci.nsIFile);
  dbFile.append("places.sqlite");

  var dbService = Cc["@mozilla.org/storage/service;1"].getService(Ci.mozIStorageService);
  var dbConnection = dbService.openDatabase(dbFile);
  
  /*
  test anno expiration (expire never)
  */
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);

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
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_DAYS);
  // these annotations should remain as they are only 6 days old
  var expirationDate = (Date.now() - (6 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_WEEKS);
  // these annotations should remain as they are only 29 days old
  var expirationDate = (Date.now() - (29 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(testURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
  annosvc.setPageAnnotation(testURI, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  annosvc.setItemAnnotation(bookmark, testAnnoName, testAnnoVal, 0, annosvc.EXPIRE_MONTHS);
  // these annotations should remain as they are only 179 days old
  var expirationDate = (Date.now() - (179 * 86400 * 1000)) * 1000;
  dbConnection.executeSimpleSQL("UPDATE moz_annos SET dateAdded = " + expirationDate);
  dbConnection.executeSimpleSQL("UPDATE moz_items_annos SET dateAdded = " + expirationDate);

  // add a uri and then remove it, to trigger expiration
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
  histsvc.addVisit(triggerURI, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
}
