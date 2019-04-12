var now = Date.now() * 1000;

// Test that importing bookmark data where a bookmark has a tag longer than 100
// chars imports everything except the tags for that bookmark.
add_task(async function() {
  let bookmarksFile = OS.Path.join(do_get_cwd().path, "bookmarks_long_tag.json");
  let bookmarksUrl = OS.Path.toFileURI(bookmarksFile);

  await BookmarkJSONUtils.importFromURL(bookmarksUrl);

  let [bookmarks] = await PlacesBackups.getBookmarksTree();
  let unsortedBookmarks = bookmarks.children[2].children;
  Assert.equal(unsortedBookmarks.length, 3);

  for (let i = 0; i < unsortedBookmarks.length; ++i) {
    let bookmark = unsortedBookmarks[i];
    Assert.equal(bookmark.charset, "UTF-16");
    Assert.equal(bookmark.dateAdded, 1554906792000);
    Assert.equal(bookmark.lastModified, 1554906792000);
    Assert.equal(bookmark.uri, `http://test${i}.com/`);
    Assert.equal(bookmark.tags, `tag${i}`);
  }
});
