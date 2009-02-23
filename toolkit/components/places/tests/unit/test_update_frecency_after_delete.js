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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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
 * Bug 455315
 * https://bugzilla.mozilla.org/show_bug.cgi?id=412132
 *
 * Ensures that the frecency of a bookmark's URI is what it should be after the
 * bookmark is deleted.
 */

const bmServ =
  Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
  getService(Ci.nsINavBookmarksService);
const histServ =
  Cc["@mozilla.org/browser/nav-history-service;1"].
  getService(Ci.nsINavHistoryService);
const lmServ =
  Cc["@mozilla.org/browser/livemark-service;2"].
  getService(Ci.nsILivemarkService);

const dbConn =
  Cc["@mozilla.org/browser/nav-history-service;1"].
  getService(Ci.nsPIPlacesDatabase).
  DBConnection;

var tests = [];

tests.push({
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after bookmark removed."].join(""),
  run: function () {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(lmItemURL), 0);

    bmServ.removeItem(bmId);

    // URI's only "bookmark" is now unvisited livemark item => frecency = 0.
    do_check_eq(getFrecency(lmItemURL), 0);
  }
});

tests.push({
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after bookmark removed."].join(""),
  run: function () {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(lmItemURL), 0);

    visit(lmItemURI);
    bmServ.removeItem(bmId);

    // URI's only "bookmark" is now *visited* livemark item => frecency != 0.
    do_check_neq(getFrecency(lmItemURL), 0);
  }
});

tests.push({
  desc: ["After removing bookmark, frecency of bookmark's URI should be zero ",
         "if URI is unvisited and no longer bookmarked."].join(""),
  run: function () {
    let url = "http://example.com/1";
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     uri(url),
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(url), 0);

    bmServ.removeItem(bmId);

    // Unvisited URI no longer bookmarked => frecency should = 0.
    do_check_eq(getFrecency(url), 0);
  }
});

tests.push({
  desc: ["After removing bookmark, frecency of bookmark's URI should not be ",
         "zero if URI is visited."].join(""),
  run: function () {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     bmURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(bmURL), 0);

    visit(bmURI);
    bmServ.removeItem(bmId);

    // *Visited* URI no longer bookmarked => frecency should != 0.
    do_check_neq(getFrecency(bmURL), 0);
  }
});

tests.push({
  desc: ["After removing bookmark, frecency of bookmark's URI should not be ",
         "zero if URI is still bookmarked."].join(""),
  run: function () {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    let bm1Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                      bmURI,
                                      bmServ.DEFAULT_INDEX,
                                      "bookmark 1 title");

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 2 title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(bmURL), 0);

    bmServ.removeItem(bm1Id);

    // URI still bookmarked => frecency should != 0.
    do_check_neq(getFrecency(bmURL), 0);
  }
});

tests.push({
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after all children removed from bookmark's ",
         "parent."].join(""),
  run: function () {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          lmItemURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(lmItemURL), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);

    // URI's only "bookmark" is now unvisited livemark item => frecency = 0.
    do_check_eq(getFrecency(lmItemURL), 0);
  }
});

tests.push({
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after all children removed from bookmark's ",
         "parent."].join(""),
  run: function () {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          lmItemURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(lmItemURL), 0);

    visit(lmItemURI);
    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);

    // URI's only "bookmark" is now *visited* livemark item => frecency != 0.
    do_check_neq(getFrecency(lmItemURL), 0);
  }
});

tests.push({
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should be zero if URI is unvisited and no longer ",
         "bookmarked."].join(""),
  run: function () {
    let url = "http://example.com/1";
    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          uri(url),
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(url), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);

    // Unvisited URI no longer bookmarked => frecency should = 0.
    do_check_eq(getFrecency(url), 0);
  }
});

tests.push({
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should not be zero if URI is visited."].join(""),
  run: function () {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(bmURL), 0);

    visit(bmURI);
    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);

    // *Visited* URI no longer bookmarked => frecency should != 0.
    do_check_neq(getFrecency(bmURL), 0);
  }
});

tests.push({
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should not be zero if URI is still ",
         "bookmarked."].join(""),
  run: function () {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    bmServ.insertBookmark(bmServ.toolbarFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 1 title");

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 2 title");

    // Bookmarked => frecency of URI should be != 0.
    do_check_neq(getFrecency(bmURL), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);

    // URI still bookmarked => frecency should != 0.
    do_check_neq(getFrecency(bmURL), 0);
  }
});

///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a livemark with a single child item.
 *
 * @param  aLmChildItemURI
 *         the URI of the livemark's single child item
 * @return the item ID of the single child item
 */
function createLivemark(aLmChildItemURI) {
  let lmItemId = lmServ.createLivemarkFolderOnly(bmServ.unfiledBookmarksFolder,
                                                 "livemark title",
                                                 uri("http://example.com/"),
                                                 uri("http://example.com/rdf"),
                                                 -1);
  let lmChildItemId = bmServ.insertBookmark(lmItemId,
                                            aLmChildItemURI,
                                            bmServ.DEFAULT_INDEX,
                                            "livemark item title");
  return lmChildItemId;
}

/**
 * Returns the frecency of a Place.
 *
 * @param  aURL
 *         the URL of a Place
 * @return the frecency of aURL
 */
function getFrecency(aURL) {
  let sql = "SELECT frecency FROM moz_places_view WHERE url = :url";
  let stmt = dbConn.createStatement(sql);
  stmt.params.url = aURL;
  do_check_true(stmt.executeStep());
  let frecency = stmt.getInt32(0);
  print("frecency=" + frecency);
  stmt.finalize();

  return frecency;
}

/**
 * Reverts the Places database to initial state in preparation for the next test.  Also
 * prints out some info about the next test.
 *
 * @param aTestIndex
 *        the index in tests of the test to prepare
 * @param aTestName
 *        a description of the test to prepare
 */
function prepTest(aTestIndex, aTestName) {
  print("Test " + aTestIndex + ": " + aTestName);
  histServ.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
  remove_all_bookmarks();
}

/**
 * Adds a visit for aURI.
 *
 * @param aURI
 *        the URI of the Place for which to add a visit
 */
function visit(aURI) {
  let visitId = histServ.addVisit(aURI,
                                  Date.now() * 1000,
                                  null,
                                  histServ.TRANSITION_BOOKMARK,
                                  false,
                                  0);
  do_check_true(visitId > 0);
}

///////////////////////////////////////////////////////////////////////////////

function run_test() {
  for (let i= 0; i < tests.length; i++) {
    prepTest(i, tests[i].desc);
    tests[i].run();
  }
}
