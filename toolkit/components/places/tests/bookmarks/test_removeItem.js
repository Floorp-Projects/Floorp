/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

function run_test() {
  // folder to hold this test
  var folderId =
    PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                       "", DEFAULT_INDEX);

  // add a bookmark to the new folder
  var bookmarkURI = uri("http://iasdjkf");
  do_check_false(PlacesUtils.bookmarks.isBookmarked(bookmarkURI));
  var bookmarkId = PlacesUtils.bookmarks.insertBookmark(folderId, bookmarkURI,
                                                        DEFAULT_INDEX, "");
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(bookmarkId), "");

  // remove the folder using removeItem
  PlacesUtils.bookmarks.removeItem(folderId);
  do_check_eq(PlacesUtils.bookmarks.getBookmarkIdsForURI(bookmarkURI).length, 0);
  do_check_false(PlacesUtils.bookmarks.isBookmarked(bookmarkURI));
  do_check_eq(PlacesUtils.bookmarks.getItemIndex(bookmarkId), -1);
}
