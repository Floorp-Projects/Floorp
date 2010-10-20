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

const bmServ = PlacesUtils.bookmarks;
const histServ = PlacesUtils.history;
const lmServ = PlacesUtils.livemarks;

let tests = [
{
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after bookmark's URI changed."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0.");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.changeBookmarkURI(aItemId, uri("http://example.com/new-uri"));
    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now unvisited livemark item => frecency = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after bookmark's URI changed."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));

    bmServ.changeBookmarkURI(aItemId, uri("http://example.com/new-uri"));
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now *visited* livemark item => frecency != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should be zero if original URI is unvisited and no longer ",
         "bookmarked."].join(""),
  run: function ()
  {
    let url = "http://example.com/1";
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     uri(url),
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(url, function(aFrecency) aFrecency > 0,
                    this.check1, this, [url, bmId]);
  },
  check1: function (aUrl, aItemId) {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.changeBookmarkURI(aItemId, uri("http://example.com/2"));

    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("Unvisited URI no longer bookmarked => frecency should = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should not be zero if original URI is visited."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     uri(bmURL),
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl, aItemId]);
  },
  check2: function (aUrl, aItemId)
  {
    bmServ.changeBookmarkURI(aItemId, uri("http://example.com/2"));
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check3, this, [aUrl]);
  },
  check3: function (aUrl)
  {
    print("*Visited* URI no longer bookmarked => frecency should != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After changing URI of bookmark, frecency of bookmark's original URI ",
         "should not be zero if original URI is still bookmarked."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bm1Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                      uri(bmURL),
                                      bmServ.DEFAULT_INDEX,
                                      "bookmark 1 title");

    let bm2Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                      uri(bmURL),
                                      bmServ.DEFAULT_INDEX,
                                      "bookmark 2 title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL, bm1Id]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.changeBookmarkURI(aItemId, uri("http://example.com/2"));
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI still bookmarked => frecency should != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: "Changing the URI of a nonexistent bookmark should fail.",
  run: function ()
  {
    function tryChange(itemId)
    {
      try {
        bmServ.changeBookmarkURI(itemId + 1, uri("http://example.com/2"));
        do_throw("Nonexistent bookmark should throw.");
      }
      catch (exc) {}
    }

    // First try a straight-up bogus item ID, one greater than the current max
    // ID.
    let stmt = DBConn().createStatement("SELECT MAX(id) FROM moz_bookmarks");
    stmt.executeStep();
    let maxId = stmt.getInt32(0);
    stmt.finalize();
    tryChange(maxId + 1);

    // Now add a bookmark, delete it, and check.
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     uri("http://example.com/"),
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    bmServ.removeItem(bmId);
    tryChange(bmId);
    run_next_test();
  }
},

];

/******************************************************************************/

function createLivemark(lmItemURI)
{
  let lmId = lmServ.createLivemarkFolderOnly(bmServ.unfiledBookmarksFolder,
                                             "livemark title",
                                             uri("http://www.mozilla.org/"),
                                             uri("http://www.mozilla.org/news.rdf"),
                                             -1);
  return bmServ.insertBookmark(lmId,
                               lmItemURI,
                               bmServ.DEFAULT_INDEX,
                               "livemark item title");
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
  do_test_pending();
  run_next_test();
}

function run_next_test()
{
  if (tests.length) {
    let test = tests.shift();
    print("\n ***Test: " + test.desc);
    waitForClearHistory(function() {
      DBConn().executeSimpleSQL("DELETE FROM moz_places");
      remove_all_bookmarks();
      test.run.call(test);
    });
  }
  else {
    do_test_finished();
  }
}
