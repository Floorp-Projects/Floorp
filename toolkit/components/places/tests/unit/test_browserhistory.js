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

// Get global history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"].
                getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
}

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].
                 getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
}

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
}

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  return (root.childCount == 1);
}

// main
function run_test() {
  var testURI = uri("http://mozilla.com");

  /**
   * addPageWithDetails
   * Adds a page to history with specific time stamp information.
   * This is used in the History migrator.
   */
  try {
    bhist.addPageWithDetails(testURI, "testURI", Date.now() * 1000);
  } catch(ex) {
    do_throw("addPageWithDetails failed");
  }

  /**
   * lastPageVisited
   * The title of the last page that was visited in a top-level window.
   */
  do_check_eq("http://mozilla.com/", bhist.lastPageVisited);

  /**
   * count
   * Indicate if there are entries in global history
   */
  do_check_eq(1, bhist.count);

  /**
   * remove a page from history
   */
  try {
    bhist.removePage(testURI);
  } catch(ex) {
    do_throw("removePage failed");
  }
  do_check_eq(0, bhist.count);
  do_check_eq("", bhist.lastPageVisited);

  /**
   * remove a bunch of pages from history
   */
  var deletedPages = [];
  deletedPages.push(uri("http://mirror1.mozilla.com"));
  deletedPages.push(uri("http://mirror2.mozilla.com"));
  deletedPages.push(uri("http://mirror3.mozilla.com"));
  deletedPages.push(uri("http://mirror4.mozilla.com"));
  deletedPages.push(uri("http://mirror5.mozilla.com"));
  deletedPages.push(uri("http://mirror6.mozilla.com"));
  deletedPages.push(uri("http://mirror7.mozilla.com"));
  deletedPages.push(uri("http://mirror8.mozilla.com"));

  try {
    for (var i = 0; i < deletedPages.length ; ++i)
      bhist.addPageWithDetails(deletedPages[i], "testURI" + (i+1),
                               Date.now() * 1000);
  } catch(ex) {
    do_throw("addPageWithDetails failed");
  }

  // annotated and bookmarked items should not be removed from moz_places
  var annoIndex = 1;
  var annoName = "testAnno";
  var annoValue = "foo";
  var bookmarkIndex = 2;
  var bookmarkName = "bar";  
  annosvc.setPageAnnotation(deletedPages[annoIndex], annoName, annoValue, 0,
                            Ci.nsIAnnotationService.EXPIRE_NEVER);
  var bookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,
      deletedPages[bookmarkIndex], bmsvc.DEFAULT_INDEX, bookmarkName);
  annosvc.setPageAnnotation(deletedPages[bookmarkIndex],
                            annoName, annoValue, 0,
                            Ci.nsIAnnotationService.EXPIRE_NEVER);

  try {
    bhist.removePages(deletedPages, deletedPages.length, false);
  } catch(ex) {
    do_throw("removePages failed");
  }
  do_check_eq(0, bhist.count);
  do_check_eq("", bhist.lastPageVisited);
  // check that bookmark and annotation still exist
  do_check_eq(bmsvc.getBookmarkURI(bookmark).spec,
              deletedPages[bookmarkIndex].spec);
  do_check_eq(annosvc.getPageAnnotation(deletedPages[bookmarkIndex], annoName),
              annoValue);
  try {
    annosvc.getPageAnnotation(deletedPages[annoIndex], annoName);
    do_throw("did not expire expire_never anno on a not bookmarked item");
  } catch(ex) {}
  // remove annotation and bookmark
  annosvc.removePageAnnotation(deletedPages[bookmarkIndex], annoName);
  bmsvc.removeItem(bookmark);
  bhist.removeAllPages();

  /**
   * removePagesByTimeframe
   * Remove all pages for a given timeframe.
   */
  // PRTime is microseconds while JS time is milliseconds
  var startDate = Date.now() * 1000;
  try {
    for (var i = 0; i < 10; ++i) {
      let testURI = uri("http://mirror" + i + ".mozilla.com");
      bhist.addPageWithDetails(testURI, "testURI" + i, startDate + i);
    }
  } catch(ex) {
    do_throw("addPageWithDetails failed");
  }
  // delete all pages except the first and the last
  bhist.removePagesByTimeframe(startDate+1, startDate+8);
  // check that we have removed correct pages
  for (var i = 0; i < 10; ++i) {
    let testURI = uri("http://mirror" + i + ".mozilla.com");
    if (i > 0 && i < 9)
      do_check_false(uri_in_db(testURI));
    else
      do_check_true(uri_in_db(testURI));
  }
  // clear remaining items and check that all pages have been removed
  bhist.removePagesByTimeframe(startDate, startDate+9);
  do_check_eq(0, bhist.count);

  /**
   * removePagesFromHost
   * Remove all pages from the given host.
   * If aEntireDomain is true, will assume aHost is a domain,
   * and remove all pages from the entire domain.
   */
  bhist.addPageWithDetails(testURI, "testURI", Date.now() * 1000);
  bhist.removePagesFromHost("mozilla.com", true);
  do_check_eq(0, bhist.count);

  // test aEntireDomain
  bhist.addPageWithDetails(testURI, "testURI", Date.now() * 1000);
  var testURI2 = uri("http://foobar.mozilla.com");
  bhist.addPageWithDetails(testURI2, "testURI2", Date.now() * 1000);
  bhist.removePagesFromHost("mozilla.com", false);
  do_check_eq(1, bhist.count);

  /**
   * removeAllPages
   * Remove all pages from global history
   */
  bhist.removeAllPages();
  do_check_eq(0, bhist.count);

  /**
   * hidePage
   * Hide the specified URL from being enumerated (and thus
   * displayed in the UI)
   *
   * if the page hasn't been visited yet, then it will be added
   * as if it was visited, and then marked as hidden
   */
  //XXX NOT IMPLEMENTED in the history service
  //bhist.addPageWithDetails(testURI, "testURI", Date.now() * 1000);
  //bhist.hidePage(testURI);
  //do_check_eq(0, bhist.count);
}
