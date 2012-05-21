/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 455315
 * https://bugzilla.mozilla.org/show_bug.cgi?id=412132
 *
 * Ensures that the frecency of a bookmark's URI is what it should be after the
 * bookmark is deleted.
 */

add_test(function removed_bookmark()
{
  do_log_info("After removing bookmark, frecency of bookmark's URI should be " +
              "zero if URI is unvisited and no longer bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                TEST_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark title");
  waitForAsyncUpdates(function ()
  {
    do_log_info("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(TEST_URI), 0);

    PlacesUtils.bookmarks.removeItem(id);

    waitForAsyncUpdates(function ()
    {
      do_log_info("Unvisited URI no longer bookmarked => frecency should = 0");
      do_check_eq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function removed_but_visited_bookmark()
{
  do_log_info("After removing bookmark, frecency of bookmark's URI should " +
              "not be zero if URI is visited.");
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
    PlacesUtils.bookmarks.removeItem(id);

    waitForAsyncUpdates(function ()
    {
      do_log_info("*Visited* URI no longer bookmarked => frecency should != 0");
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function remove_bookmark_still_bookmarked()
{
  do_log_info("After removing bookmark, frecency of bookmark's URI should ",
              "not be zero if URI is still bookmarked.");
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

    PlacesUtils.bookmarks.removeItem(id1);

    waitForAsyncUpdates(function ()
    {
      do_log_info("URI still bookmarked => frecency should != 0");
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function cleared_parent_of_visited_bookmark()
{
  do_log_info("After removing all children from bookmark's parent, frecency " +
              "of bookmark's URI should not be zero if URI is visited.");
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
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);

    waitForAsyncUpdates(function ()
    {
      do_log_info("*Visited* URI no longer bookmarked => frecency should != 0");
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
});

add_test(function cleared_parent_of_bookmark_still_bookmarked()
{
  do_log_info("After removing all children from bookmark's parent, frecency " +
              "of bookmark's URI should not be zero if URI is still " +
              "bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let id1 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
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

    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);

    waitForAsyncUpdates(function ()
    {
      // URI still bookmarked => frecency should != 0.
      do_check_neq(frecencyForUrl(TEST_URI), 0);

      remove_all_bookmarks();
      waitForClearHistory(run_next_test);
    });
  });
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
