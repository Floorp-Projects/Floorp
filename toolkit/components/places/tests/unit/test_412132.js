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

add_test(function unvisited_bookmarked_livemarkItem()
{
  do_log_info("Frecency of unvisited, separately bookmarked livemark item's " +
              "URI should be zero after bookmark's URI changed.");

  // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
  // only item.
  const TEST_URI = NetUtil.newURI("http://example.com/livemark-item");
  createLivemark(TEST_URI);
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0.");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    PlacesUtils.bookmarks.changeBookmarkURI(id, NetUtil.newURI("http://example.com/new-uri"));

    waitForAsyncUpdates(function ()
    {
      do_log_info("URI's only bookmark is now unvisited livemark item => frecency = 0");
      do_check_eq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function visited_bookmarked_livemarkItem()
{
  do_log_info("Frecency of visited, separately bookmarked livemark item's " +
              "URI should not be zero after bookmark's URI changed.");

  // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
  // only item.
  const TEST_URI = NetUtil.newURI("http://example.com/livemark-item");
  createLivemark(TEST_URI);
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    visit(TEST_URI);

    PlacesUtils.bookmarks.changeBookmarkURI(id, uri("http://example.com/new-uri"));

    waitForAsyncUpdates(function ()
    {
      do_log_info("URI's only bookmark is now *visited* livemark item => frecency != 0");
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function changeuri_unvisited_bookmark()
{
  do_log_info("After changing URI of bookmark, frecency of bookmark's " +
              "original URI should be zero if original URI is unvisited and " +
              "no longer bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    PlacesUtils.bookmarks.changeBookmarkURI(id, uri("http://example.com/2"));

    waitForAsyncUpdates(function ()
    {
      do_log_info("Unvisited URI no longer bookmarked => frecency should = 0");
      do_check_eq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function changeuri_visited_bookmark()
{
  do_log_info("After changing URI of bookmark, frecency of bookmark's " +
              "original URI should not be zero if original URI is visited.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    visit(TEST_URI);

    waitForAsyncUpdates(function ()
    {
      PlacesUtils.bookmarks.changeBookmarkURI(id, uri("http://example.com/2"));
      waitForAsyncUpdates(function ()
      {
        do_log_info("*Visited* URI no longer bookmarked => frecency should != 0");
        do_check_neq(frecencyForUrl(TEST_URI), 0);

        remove_all_bookmarks();
        waitForClearHistory(run_next_test);
      });
    });
  });
});

add_test(function changeuri_bookmark_still_bookmarked()
{
  do_log_info("After changing URI of bookmark, frecency of bookmark's " +
              "original URI should not be zero if original URI is still " +
              "bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let id1 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                 TEST_URI,
                                                 PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                 "bookmark 1 title");
  let id2 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                 TEST_URI,
                                                 PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                 "bookmark 2 title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    PlacesUtils.bookmarks.changeBookmarkURI(id1, uri("http://example.com/2"));
    waitForAsyncUpdates(function ()
    {
      do_log_info("URI still bookmarked => frecency should != 0");
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function changeuri_nonexistent_bookmark()
{
  do_log_info("Changing the URI of a nonexistent bookmark should fail.");
    function tryChange(itemId)
    {
      try {
        PlacesUtils.bookmarks.changeBookmarkURI(itemId + 1, uri("http://example.com/2"));
        do_throw("Nonexistent bookmark should throw.");
      }
      catch (ex) {}
    }

    // First try a straight-up bogus item ID, one greater than the current max
    // ID.
    let stmt = DBConn().createStatement("SELECT MAX(id) FROM moz_bookmarks");
    stmt.executeStep();
    let maxId = stmt.getInt32(0);
    stmt.finalize();
    tryChange(maxId + 1);

    // Now add a bookmark, delete it, and check.
    let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  uri("http://example.com/"),
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  "bookmark title");
    PlacesUtils.bookmarks.removeItem(id);
    tryChange(id);

    remove_all_bookmarks();
    waitForClearHistory(run_next_test);
});

///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a livemark with a single child item.
 *
 * @param  aChildURI
 *         the URI of the livemark's single child item
 * @return the item ID of the single child item
 */
function createLivemark(aChildURI)
{
  let livemarkId = PlacesUtils.livemarks.createLivemarkFolderOnly(
    PlacesUtils.unfiledBookmarksFolderId, "livemark title",
    uri("http://example.com/"), uri("http://example.com/rdf"), -1
  );
  return PlacesUtils.bookmarks.insertBookmark(livemarkId,
                                              aChildURI,
                                              PlacesUtils.bookmarks.DEFAULT_INDEX,
                                              "livemark item title");
}

/**
 * Adds a visit for aURI.
 *
 * @param aURI
 *        the URI of the Place for which to add a visit
 */
function visit(aURI)
{
  PlacesUtils.history.addVisit(aURI, Date.now() * 1000, null,
                               PlacesUtils.history.TRANSITION_BOOKMARK,
                               false, 0);
}

///////////////////////////////////////////////////////////////////////////////

function run_test()
{
  run_next_test();
}
