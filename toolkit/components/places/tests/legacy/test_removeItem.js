/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

add_task(async function test_removeItem() {
  // folder to hold this test
  var folderId =
    PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                       "", DEFAULT_INDEX);

  // add a bookmark to the new folder
  var bookmarkURI = uri("http://iasdjkf");
  Assert.equal(null, await PlacesUtils.bookmarks.fetch({url: bookmarkURI}));
  var bookmarkId = PlacesUtils.bookmarks.insertBookmark(folderId, bookmarkURI,
                                                        DEFAULT_INDEX, "");
  Assert.equal(PlacesUtils.bookmarks.getItemTitle(bookmarkId), "");
  Assert.ok(await PlacesUtils.bookmarks.fetch({url: bookmarkURI}));

  // remove the folder using removeItem
  PlacesUtils.bookmarks.removeItem(folderId);
  Assert.equal(PlacesUtils.bookmarks.getBookmarkIdsForURI(bookmarkURI).length, 0);
  Assert.equal(null, await PlacesUtils.bookmarks.fetch({url: bookmarkURI}));
  Assert.equal(PlacesUtils.bookmarks.getItemIndex(bookmarkId), -1);
});
