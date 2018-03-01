/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/BookmarkJSONUtils.jsm");

// Exported bookmarks file pointer.
var bookmarksExportedFile;

add_task(async function test_import_bookmarks() {
  let bookmarksFile = OS.Path.join(do_get_cwd().path, "bookmarks_corrupt.json");

  await BookmarkJSONUtils.importFromFile(bookmarksFile, { replace: true });
  await PlacesTestUtils.promiseAsyncUpdates();

  let bookmarks = await PlacesUtils.promiseBookmarksTree(PlacesUtils.bookmarks.menuGuid);

  Assert.equal(bookmarks.children.length, 1, "should only be one bookmark");
  let bookmark = bookmarks.children[0];
  Assert.equal(bookmark.guid, "OCyeUO5uu9FH", "should have correct guid");
  Assert.equal(bookmark.title, "Customize Firefox", "should have correct title");
  Assert.equal(bookmark.uri, "http://en-us.www.mozilla.com/en-US/firefox/customize/",
    "should have correct uri");
});
