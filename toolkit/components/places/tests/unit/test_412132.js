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

/*
 * TEST DESCRIPTION:
 *
 * Tests patch to Bug 412132:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=412132
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

var defaultBookmarksMaxId;
var dbConn;
var tests = [];

tests.push({
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after bookmark's URI changed."].join(""),
  run: function ()
  {
    var lmItemURL;
    var lmItemURI;
    var bmId;
    var frecencyBefore;
    var frecencyAfter;

    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    lmItemURL = "http://example.com/livemark-item";
    lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 lmItemURI,
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    frecencyBefore = getFrecency(lmItemURL);
    do_check_neq(frecencyBefore, 0);

    bmServ.changeBookmarkURI(bmId, uri("http://example.com/new-uri"));

    // URI's only "bookmark" is now unvisited livemark item => frecency = 0.
    frecencyAfter = getFrecency(lmItemURL);
    do_check_eq(frecencyAfter, 0);
  }
});

tests.push({
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after bookmark's URI changed."].join(""),
  run: function ()
  {
    var lmItemURL;
    var lmItemURI;
    var bmId;
    var frecencyBefore;
    var frecencyAfter;

    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    lmItemURL = "http://example.com/livemark-item";
    lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 lmItemURI,
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    frecencyBefore = getFrecency(lmItemURL);
    do_check_neq(frecencyBefore, 0);

    visit(lmItemURI);

    bmServ.changeBookmarkURI(bmId, uri("http://example.com/new-uri"));

    // URI's only "bookmark" is now *visited* livemark item => frecency != 0.
    frecencyAfter = getFrecency(lmItemURL);
    do_check_neq(frecencyAfter, 0);
  }
});

tests.push({
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should be zero if original URI is unvisited and no longer ",
         "bookmarked."].join(""),
  run: function ()
  {
    var url;
    var bmId;
    var frecencyBefore;
    var frecencyAfter;

    url = "http://example.com/1";
    bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 uri(url),
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    frecencyBefore = getFrecency(url);
    do_check_neq(frecencyBefore, 0);

    bmServ.changeBookmarkURI(bmId, uri("http://example.com/2"));

    // Unvisited URI no longer bookmarked => frecency should = 0.
    frecencyAfter = getFrecency(url);
    do_check_eq(frecencyAfter, 0);
  }
});

tests.push({
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should not be zero if original URI is visited."].join(""),
  run: function ()
  {
    var bmURL;
    var bmURI;
    var bmId;
    var frecencyBefore;
    var frecencyAfter;

    bmURL = "http://example.com/1";
    bmURI = uri(bmURL);

    bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 bmURI,
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark title");

    // Bookmarked => frecency of URI should be != 0.
    frecencyBefore = getFrecency(bmURL);
    do_check_neq(frecencyBefore, 0);

    visit(bmURI);
    bmServ.changeBookmarkURI(bmId, uri("http://example.com/2"));

    // *Visited* URI no longer bookmarked => frecency should != 0.
    frecencyAfter = getFrecency(bmURL);
    do_check_neq(frecencyAfter, 0);
  }
});

tests.push({
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should not be zero if original URI is still bookmarked."].join(""),
  run: function ()
  {
    var bmURL;
    var bmURI;
    var bm1Id;
    var bm2Id;
    var frecencyBefore;
    var frecencyAfter;

    bmURL = "http://example.com/1";
    bmURI = uri(bmURL);

    bm1Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 bmURI,
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark 1 title");

    bm2Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                  bmURI,
                                  bmServ.DEFAULT_INDEX,
                                  "bookmark 2 title");


    // Bookmarked => frecency of URI should be != 0.
    frecencyBefore = getFrecency(bmURL);
    do_check_neq(frecencyBefore, 0);

    bmServ.changeBookmarkURI(bm1Id, uri("http://example.com/2"));

    // URI still bookmarked => frecency should != 0.
    frecencyAfter = getFrecency(bmURL);
    do_check_neq(frecencyAfter, 0);
  }
});

tests.push({
  desc: "Changing the URI of a nonexistent bookmark should fail.",
  run: function ()
  {
    var stmt;
    var maxId;
    var bmId;

    function tryChange(itemId)
    {
      var failed;

      failed= false;
      try
      {
        bmServ.changeBookmarkURI(itemId + 1, uri("http://example.com/2"));
      }
      catch (exc)
      {
        failed= true;
      }
      do_check_true(failed);
    }

    // First try a straight-up bogus item ID, one greater than the current max
    // ID.
    stmt = dbConn.createStatement("SELECT MAX(id) FROM moz_bookmarks");
    stmt.executeStep();
    maxId = stmt.getInt32(0);
    stmt.finalize();
    tryChange(maxId + 1);

    // Now add a bookmark, delete it, and check.
    bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                 uri("http://example.com/"),
                                 bmServ.DEFAULT_INDEX,
                                 "bookmark title");
    bmServ.removeItem(bmId);
    tryChange(bmId);
  }
});

/******************************************************************************/

function createLivemark(lmItemURI)
{
  var lmId;
  var lmItemId;

  lmId = lmServ.createLivemarkFolderOnly(bmServ.unfiledBookmarksFolder,
                                         "livemark title",
                                         uri("http://www.mozilla.org/"),
                                         uri("http://www.mozilla.org/news.rdf"),
                                         -1);
  lmItemId = bmServ.insertBookmark(lmId,
                                   lmItemURI,
                                   bmServ.DEFAULT_INDEX,
                                   "livemark item title");
  return lmItemId;
}

function getFrecency(url)
{
  var sql;
  var stmt;
  var frecency;

  sql = "SELECT frecency FROM moz_places_view WHERE url = ?1";
  stmt = dbConn.createStatement(sql);
  stmt.bindUTF8StringParameter(0, url);
  do_check_true(stmt.executeStep());
  frecency = stmt.getInt32(0);
  print("frecency=" + frecency);
  stmt.finalize();

  return frecency;
}

function prepTest(testIndex, testName)
{
  print("Test " + testIndex + ": " + testName);
  histServ.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
  dbConn.executeSimpleSQL("DELETE FROM moz_places_view");
  dbConn.executeSimpleSQL("DELETE FROM moz_bookmarks WHERE id > " +
                          defaultBookmarksMaxId);
}

function visit(uri)
{
  histServ.addVisit(uri,
                    Date.now() * 1000,
                    null,
                    histServ.TRANSITION_BOOKMARK,
                    false,
                    0);
}

/******************************************************************************/

function run_test()
{
  var stmt;

  dbConn =
    Cc["@mozilla.org/browser/nav-history-service;1"].
    getService(Ci.nsPIPlacesDatabase).
    DBConnection;

  stmt = dbConn.createStatement("SELECT MAX(id) FROM moz_bookmarks");
  stmt.executeStep();
  defaultBookmarksMaxId = stmt.getInt32(0);
  stmt.finalize();
  do_check_true(defaultBookmarksMaxId > 0);

  for (let i= 0; i < tests.length; i++)
  {
    prepTest(i, tests[i].desc);
    tests[i].run();
  }
}
