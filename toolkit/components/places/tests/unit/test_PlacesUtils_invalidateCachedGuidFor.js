add_task(function* () {
  do_print("Add a bookmark.");
  let bm = yield PlacesUtils.bookmarks.insert({ url: "http://example.com/",
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let id = yield PlacesUtils.promiseItemId(bm.guid);
  Assert.equal((yield PlacesUtils.promiseItemGuid(id)), bm.guid);

  // Ensure invalidating a non-existent itemId doesn't throw.
  PlacesUtils.invalidateCachedGuidFor(null);
  PlacesUtils.invalidateCachedGuidFor(9999);

  do_print("Change the GUID.");
  yield PlacesUtils.withConnectionWrapper("test", Task.async(function*(db) {
    yield db.execute("UPDATE moz_bookmarks SET guid = :guid WHERE id = :id",
                     { guid: "123456789012", id});
  }));
  // The cache should still point to the wrong id.
  Assert.equal((yield PlacesUtils.promiseItemGuid(id)), bm.guid);

  do_print("Invalidate the cache.");
  PlacesUtils.invalidateCachedGuidFor(id);
  Assert.equal((yield PlacesUtils.promiseItemGuid(id)), "123456789012");
  Assert.equal((yield PlacesUtils.promiseItemId("123456789012")), id);
  yield Assert.rejects(PlacesUtils.promiseItemId(bm.guid), /no item found for the given GUID/);
});
