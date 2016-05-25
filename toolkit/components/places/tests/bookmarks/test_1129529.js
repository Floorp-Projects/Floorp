var now = Date.now() * 1000;

// Test that importing bookmark data where a bookmark has a tag longer than 100
// chars imports everything except the tags for that bookmark.
add_task(function* () {
  let aData = {
    guid: "root________",
    index: 0,
    id: 1,
    type: "text/x-moz-place-container",
    dateAdded: now,
    lastModified: now,
    root: "placesRoot",
    children: [{
      guid: "unfiled_____",
      index: 0,
      id: 2,
      type: "text/x-moz-place-container",
      dateAdded: now,
      lastModified: now,
      root: "unfiledBookmarksFolder",
      children: [
        {
          guid: "___guid1____",
          index: 0,
          id: 3,
          charset: "UTF-8",
          tags: "tag0",
          type: "text/x-moz-place",
          dateAdded: now,
          lastModified: now,
          uri: "http://test0.com/"
        },
        {
          guid: "___guid2____",
          index: 1,
          id: 4,
          charset: "UTF-8",
          tags: "tag1," + "a" + "0123456789".repeat(10), // 101 chars
          type: "text/x-moz-place",
          dateAdded: now,
          lastModified: now,
          uri: "http://test1.com/"
        },
        {
          guid: "___guid3____",
          index: 2,
          id: 5,
          charset: "UTF-8",
          tags: "tag2",
          type: "text/x-moz-place",
          dateAdded: now,
          lastModified: now,
          uri: "http://test2.com/"
        }
      ]
    }]
  };

  let contentType = "application/json";
  let uri = "data:" + contentType + "," + JSON.stringify(aData);
  yield BookmarkJSONUtils.importFromURL(uri, false);

  let [bookmarks, count] = yield PlacesBackups.getBookmarksTree();
  let unsortedBookmarks = bookmarks.children[2].children;
  Assert.equal(unsortedBookmarks.length, 3);

  for (let i = 0; i < unsortedBookmarks.length; ++i) {
    let bookmark = unsortedBookmarks[i];
    Assert.equal(bookmark.charset, "UTF-8");
    Assert.equal(bookmark.dateAdded, now);
    Assert.equal(bookmark.lastModified, now);
    Assert.equal(bookmark.uri, "http://test" + i + ".com/");
    Assert.equal(bookmark.tags, "tag" + i);
  }
});
