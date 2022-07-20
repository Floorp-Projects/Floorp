/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for bug #801450

// Get Services
const { BookmarkHTMLUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BookmarkHTMLUtils.sys.mjs"
);

add_task(async function test_bookmarks_html_singleframe() {
  let bookmarksFile = PathUtils.join(
    do_get_cwd().path,
    "bookmarks_html_singleframe.html"
  );
  await BookmarkHTMLUtils.importFromFile(bookmarksFile, { replace: true });

  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.menuGuid).root;
  Assert.equal(root.childCount, 1);
  let folder = root.getChild(0);
  PlacesUtils.asContainer(folder).containerOpen = true;
  Assert.equal(folder.title, "Subtitle");
  Assert.equal(folder.childCount, 1);
  let bookmark = folder.getChild(0);
  Assert.equal(bookmark.uri, "http://www.mozilla.org/");
  Assert.equal(bookmark.title, "Mozilla");
  folder.containerOpen = false;
});
