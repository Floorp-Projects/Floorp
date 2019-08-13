/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function changeGuid(db, id, newGuid) {
  await db.execute(
    `UPDATE moz_bookmarks SET
       guid = :newGuid
     WHERE id = :id`,
    { id, newGuid }
  );
}

add_task(async function test_invalidateCachedGuids() {
  info("Add bookmarks");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        guid: "bookmarkAAAA",
        url: "http://example.com/a",
        title: "A",
      },
      {
        guid: "bookmarkBBBB",
        url: "http://example.com/b",
        title: "B",
      },
    ],
  });

  let ids = await PlacesUtils.promiseManyItemIds([
    "bookmarkAAAA",
    "bookmarkBBBB",
  ]);
  Assert.equal(
    await PlacesUtils.promiseItemGuid(ids.get("bookmarkAAAA")),
    "bookmarkAAAA"
  );
  Assert.equal(
    await PlacesUtils.promiseItemGuid(ids.get("bookmarkBBBB")),
    "bookmarkBBBB"
  );

  info("Change GUIDs");
  await PlacesUtils.withConnectionWrapper(
    "test_invalidateCachedGuids",
    async function(db) {
      await db.executeTransaction(async function() {
        await changeGuid(db, ids.get("bookmarkAAAA"), "bookmarkCCCC");
        await changeGuid(db, ids.get("bookmarkBBBB"), "bookmarkDDDD");
      });
    }
  );
  Assert.equal(
    await PlacesUtils.promiseItemId("bookmarkAAAA"),
    ids.get("bookmarkAAAA")
  );
  Assert.equal(
    await PlacesUtils.promiseItemId("bookmarkBBBB"),
    ids.get("bookmarkBBBB")
  );

  info("Invalidate the cache");
  PlacesUtils.invalidateCachedGuids();

  let newIds = await PlacesUtils.promiseManyItemIds([
    "bookmarkCCCC",
    "bookmarkDDDD",
  ]);
  Assert.equal(
    await PlacesUtils.promiseItemGuid(newIds.get("bookmarkCCCC")),
    "bookmarkCCCC"
  );
  Assert.equal(
    await PlacesUtils.promiseItemGuid(newIds.get("bookmarkDDDD")),
    "bookmarkDDDD"
  );
  await Assert.rejects(
    PlacesUtils.promiseItemId("bookmarkAAAA"),
    /no item found for the given GUID/
  );
  await Assert.rejects(
    PlacesUtils.promiseItemId("bookmarkBBBB"),
    /no item found for the given GUID/
  );
});
