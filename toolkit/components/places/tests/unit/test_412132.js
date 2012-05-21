/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * TEST DESCRIPTION:
 *
 * Tests patch to Bug 412132:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=412132
 */

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
