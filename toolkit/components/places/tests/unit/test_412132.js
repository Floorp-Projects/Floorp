/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

const TEST_URL0 = "http://example.com/";
const TEST_URL1 = "http://example.com/1";
const TEST_URL2 = "http://example.com/2";

add_task(async function changeuri_unvisited_bookmark() {
  info(
    "After changing URI of bookmark, frecency of bookmark's " +
      "original URI should be zero if original URI is unvisited and " +
      "no longer bookmarked."
  );
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URL1,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.notEqual(
    frecencyForUrl(TEST_URL1),
    0,
    "Bookmarked => frecency of URI should be != 0"
  );

  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    url: TEST_URL2,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.equal(
    frecencyForUrl(TEST_URL1),
    0,
    "Unvisited URI no longer bookmarked => frecency should = 0"
  );

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function changeuri_visited_bookmark() {
  info(
    "After changing URI of bookmark, frecency of bookmark's " +
      "original URI should not be zero if original URI is visited."
  );
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URL1,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.notEqual(
    frecencyForUrl(TEST_URL1),
    0,
    "Bookmarked => frecency of URI should be != 0"
  );

  await PlacesTestUtils.addVisits(TEST_URL1);

  await PlacesTestUtils.promiseAsyncUpdates();

  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    url: TEST_URL2,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.notEqual(
    frecencyForUrl(TEST_URL1),
    0,
    "*Visited* URI no longer bookmarked => frecency should != 0"
  );

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function changeuri_bookmark_still_bookmarked() {
  info(
    "After changing URI of bookmark, frecency of bookmark's " +
      "original URI should not be zero if original URI is still " +
      "bookmarked."
  );
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 1 title",
    url: TEST_URL1,
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark 2 title",
    url: TEST_URL1,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  Assert.notEqual(
    frecencyForUrl(TEST_URL1),
    0,
    "Bookmarked => frecency of URI should be != 0"
  );

  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    url: TEST_URL2,
  });

  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI still bookmarked => frecency should != 0");
  Assert.notEqual(frecencyForUrl(TEST_URL2), 0);

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

add_task(async function changeuri_nonexistent_bookmark() {
  // Try a bogus guid.
  await Assert.rejects(
    PlacesUtils.bookmarks.update({
      guid: "ABCDEDFGHIJK",
      url: TEST_URL2,
    }),
    /No bookmarks found for the provided GUID/,
    "Changing the URI of a non-existent bookmark should fail."
  );

  // Now add a bookmark, delete it, and check.
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark title",
    url: TEST_URL0,
  });

  await PlacesUtils.bookmarks.remove(bookmark.guid);

  await Assert.rejects(
    PlacesUtils.bookmarks.update({
      guid: bookmark.guid,
      url: TEST_URL2,
    }),
    /No bookmarks found for the provided GUID/,
    "Changing the URI of a non-existent bookmark should fail."
  );

  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});
