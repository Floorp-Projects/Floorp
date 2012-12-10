/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for bug #801450

// Get Services
Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

function run_test()
{
  run_next_test();
}

add_task(function test_bookmarks_html_singleframe()
{
  let bookmarksFile = do_get_file("bookmarks_html_singleframe.html");
  yield BookmarkHTMLUtils.importFromFile(bookmarksFile, true);

  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarksMenuFolderId).root;
  do_check_eq(root.childCount, 1);
  let folder = root.getChild(0);
  PlacesUtils.asContainer(folder).containerOpen = true;
  do_check_eq(folder.title, "Subtitle");
  do_check_eq(folder.childCount, 1);
  let bookmark = folder.getChild(0);
  do_check_eq(bookmark.uri, "http://www.mozilla.org/");
  do_check_eq(bookmark.title, "Mozilla");
  folder.containerOpen = false;
});
