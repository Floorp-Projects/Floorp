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

add_task(async function removed_bookmark() {
  info(
    "After removing bookmark, frecency of bookmark's URI should be " +
      "zero if URI is unvisited and no longer bookmarked."
  );
  const TEST_URI = Services.io.newURI("http://example.com/1");
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI,
  });

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Bookmarked => frecency of URI should be != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.remove(bm);

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Unvisited URI no longer bookmarked => frecency should = 0");
  Assert.equal(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function removed_but_visited_bookmark() {
  info(
    "After removing bookmark, frecency of bookmark's URI should " +
      "not be zero if URI is visited."
  );
  const TEST_URI = Services.io.newURI("http://example.com/1");
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI,
  });

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Bookmarked => frecency of URI should be != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesTestUtils.addVisits(TEST_URI);
  await PlacesUtils.bookmarks.remove(bm);

  await PlacesTestUtils.promiseAsyncUpdates();
  info("*Visited* URI no longer bookmarked => frecency should != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function remove_bookmark_still_bookmarked() {
  info(
    "After removing bookmark, frecency of bookmark's URI should " +
      "not be zero if URI is still bookmarked."
  );
  const TEST_URI = Services.io.newURI("http://example.com/1");
  let bm1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 1 title",
    url: TEST_URI,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 2 title",
    url: TEST_URI,
  });

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Bookmarked => frecency of URI should be != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.remove(bm1);

  await PlacesTestUtils.promiseAsyncUpdates();
  info("URI still bookmarked => frecency should != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function cleared_parent_of_visited_bookmark() {
  info(
    "After removing all children from bookmark's parent, frecency " +
      "of bookmark's URI should not be zero if URI is visited."
  );
  const TEST_URI = Services.io.newURI("http://example.com/1");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URI,
  });

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Bookmarked => frecency of URI should be != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesTestUtils.addVisits(TEST_URI);
  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesTestUtils.promiseAsyncUpdates();
  info("*Visited* URI no longer bookmarked => frecency should != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function cleared_parent_of_bookmark_still_bookmarked() {
  info(
    "After removing all children from bookmark's parent, frecency " +
      "of bookmark's URI should not be zero if URI is still " +
      "bookmarked."
  );
  const TEST_URI = Services.io.newURI("http://example.com/1");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "bookmark 1 title",
    url: TEST_URI,
  });

  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bookmark 2 folder",
  });
  await PlacesUtils.bookmarks.insert({
    title: "bookmark 2 title",
    parentGuid: folder.guid,
    url: TEST_URI,
  });

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Bookmarked => frecency of URI should be != 0");
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.remove(folder);
  await PlacesTestUtils.promiseAsyncUpdates();
  // URI still bookmarked => frecency should != 0.
  Assert.notEqual(frecencyForUrl(TEST_URI), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});
