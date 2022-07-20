/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests for importing a corrupt json file.
 *
 * The corrupt json file attempts to import into:
 *   - the menu folder:
 *     - A bookmark with an invalid type.
 *     - A valid bookmark.
 *     - A bookmark with an invalid url.
 *   - the toolbar folder:
 *     - A bookmark with an invalid url.
 *
 * The menu case ensure that we strip out invalid bookmarks, but retain valid
 * ones.
 * The toolbar case ensures that if no valid bookmarks remain, then we do not
 * throw an error.
 */

const { BookmarkJSONUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BookmarkJSONUtils.sys.mjs"
);

add_task(async function test_import_bookmarks() {
  let bookmarksFile = PathUtils.join(
    do_get_cwd().path,
    "bookmarks_corrupt.json"
  );

  await BookmarkJSONUtils.importFromFile(bookmarksFile, { replace: true });
  await PlacesTestUtils.promiseAsyncUpdates();

  let bookmarks = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.menuGuid
  );

  Assert.equal(
    bookmarks.children.length,
    1,
    "should only be one bookmark in the menu"
  );
  let bookmark = bookmarks.children[0];
  Assert.equal(bookmark.guid, "OCyeUO5uu9FH", "should have correct guid");
  Assert.equal(
    bookmark.title,
    "Customize Firefox",
    "should have correct title"
  );
  Assert.equal(
    bookmark.uri,
    "http://en-us.www.mozilla.com/en-US/firefox/customize/",
    "should have correct uri"
  );

  bookmarks = await PlacesUtils.promiseBookmarksTree(
    PlacesUtils.bookmarks.toolbarGuid
  );

  Assert.ok(
    !bookmarks.children,
    "should not have any bookmarks in the toolbar"
  );
});
