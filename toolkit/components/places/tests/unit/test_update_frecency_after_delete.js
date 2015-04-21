/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

add_task(function* removed_bookmark() {
  do_print("After removing bookmark, frecency of bookmark's URI should be " +
           "zero if URI is unvisited and no longer bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI
  });

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Bookmarked => frecency of URI should be != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.remove(bm);

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Unvisited URI no longer bookmarked => frecency should = 0");
  do_check_eq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* removed_but_visited_bookmark() {
  do_print("After removing bookmark, frecency of bookmark's URI should " +
           "not be zero if URI is visited.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI
  });

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Bookmarked => frecency of URI should be != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesTestUtils.addVisits(TEST_URI);
  yield PlacesUtils.bookmarks.remove(bm);

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("*Visited* URI no longer bookmarked => frecency should != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* remove_bookmark_still_bookmarked() {
  do_print("After removing bookmark, frecency of bookmark's URI should " +
           "not be zero if URI is still bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let bm1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 1 title",
    url: TEST_URI
  });
  let bm2 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 2 title",
    url: TEST_URI
  });

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Bookmarked => frecency of URI should be != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.remove(bm1);

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("URI still bookmarked => frecency should != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* cleared_parent_of_visited_bookmark() {
  do_print("After removing all children from bookmark's parent, frecency " +
           "of bookmark's URI should not be zero if URI is visited.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI
  });

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Bookmarked => frecency of URI should be != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesTestUtils.addVisits(TEST_URI);
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("*Visited* URI no longer bookmarked => frecency should != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});

add_task(function* cleared_parent_of_bookmark_still_bookmarked() {
  do_print("After removing all children from bookmark's parent, frecency " +
           "of bookmark's URI should not be zero if URI is still " +
           "bookmarked.");
  const TEST_URI = NetUtil.newURI("http://example.com/1");
  let bm1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "bookmark 1 title",
    url: TEST_URI
  });

  let folder = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bookmark 2 folder"
  });
  let bm2 = yield PlacesUtils.bookmarks.insert({
    title: "bookmark 2 title",
    parentGuid: folder.guid,
    url: TEST_URI
  });

  yield PlacesTestUtils.promiseAsyncUpdates();
  do_print("Bookmarked => frecency of URI should be != 0");
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.remove(folder);
  yield PlacesTestUtils.promiseAsyncUpdates();
  // URI still bookmarked => frecency should != 0.
  do_check_neq(frecencyForUrl(TEST_URI), 0);

  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
});
