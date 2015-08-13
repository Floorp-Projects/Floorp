
// Bug 1192692 - promiseBookmarksTree caches items without adding observers to
// invalidate the cache.
add_task(function* boookmarks_tree_cache() {
  // Note that for this test to be effective, it needs to use the "old" sync
  // bookmarks methods - using, eg, PlacesUtils.bookmarks.insert() doesn't
  // demonstrate the problem as it indirectly arranges for the observers to
  // be added.
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                uri("http://example.com"),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "A title");
  yield PlacesUtils.promiseBookmarksTree();

  PlacesUtils.bookmarks.removeItem(id);

  yield Assert.rejects(PlacesUtils.promiseItemGuid(id));
});
