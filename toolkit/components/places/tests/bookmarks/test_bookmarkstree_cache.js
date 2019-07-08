// Bug 1192692 - promiseBookmarksTree caches items without adding observers to
// invalidate the cache.
add_task(async function boookmarks_tree_cache() {
  // Note that for this test to be effective, it needs to use the "old" sync
  // bookmarks methods - using, eg, PlacesUtils.bookmarks.insert() doesn't
  // demonstrate the problem as it indirectly arranges for the observers to
  // be added.
  let id = PlacesUtils.bookmarks.insertBookmark(
    await PlacesUtils.promiseItemId(PlacesUtils.bookmarks.unfiledGuid),
    uri("http://example.com"),
    PlacesUtils.bookmarks.DEFAULT_INDEX,
    "A title"
  );

  await PlacesUtils.promiseBookmarksTree();

  PlacesUtils.bookmarks.removeItem(id);

  await Assert.rejects(
    PlacesUtils.promiseItemGuid(id),
    /no item found for the given itemId/
  );
});
