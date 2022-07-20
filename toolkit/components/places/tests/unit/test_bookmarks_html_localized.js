/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BookmarkHTMLUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/BookmarkHTMLUtils.sys.mjs"
);

add_task(async function setup_l10n() {
  // A single localized string.
  const mockSource = L10nFileSource.createMock(
    "test",
    "app",
    ["en-US"],
    "/localization/{locale}/",
    [
      {
        path: "/localization/en-US/bookmarks_html_localized.ftl",
        source: `
bookmarks-html-localized-folder = Localized Folder
bookmarks-html-localized-bookmark = Localized Bookmark
`,
      },
    ]
  );

  L10nRegistry.getInstance().registerSources([mockSource]);
});

add_task(async function test_bookmarks_html_localized() {
  let bookmarksFile = PathUtils.join(
    do_get_cwd().path,
    "bookmarks_html_localized.html"
  );
  await BookmarkHTMLUtils.importFromFile(bookmarksFile, { replace: true });

  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.menuGuid).root;
  Assert.equal(root.childCount, 1);
  let folder = root.getChild(0);
  PlacesUtils.asContainer(folder).containerOpen = true;
  // Folder title is localized.
  Assert.equal(folder.title, "Localized Folder");
  Assert.equal(folder.childCount, 1);
  let bookmark = folder.getChild(0);
  Assert.equal(bookmark.uri, "http://www.mozilla.com/firefox/help/");
  // Bookmark title is localized.
  Assert.equal(bookmark.title, "Localized Bookmark");
  folder.containerOpen = false;
});
