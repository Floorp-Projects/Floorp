add_task(async function() {
  info("Add a bookmark.");
  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://example.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let id = await PlacesUtils.promiseItemId(bm.guid);
  Assert.equal(await PlacesUtils.promiseItemGuid(id), bm.guid);

  // Ensure invalidating a non-existent itemId doesn't throw.
  PlacesUtils.invalidateCachedGuidFor(null);
  PlacesUtils.invalidateCachedGuidFor(9999);

  info("Change the GUID.");
  await PlacesUtils.withConnectionWrapper("test", async function(db) {
    await db.execute("UPDATE moz_bookmarks SET guid = :guid WHERE id = :id", {
      guid: "123456789012",
      id,
    });
  });
  // The cache should still point to the wrong id.
  Assert.equal(await PlacesUtils.promiseItemGuid(id), bm.guid);

  info("Invalidate the cache.");
  PlacesUtils.invalidateCachedGuidFor(id);
  Assert.equal(await PlacesUtils.promiseItemGuid(id), "123456789012");
  Assert.equal(await PlacesUtils.promiseItemId("123456789012"), id);
  await Assert.rejects(
    PlacesUtils.promiseItemId(bm.guid),
    /no item found for the given GUID/
  );
});
