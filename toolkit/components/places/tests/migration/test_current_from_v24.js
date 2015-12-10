/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* setup() {
  yield setupPlacesDatabase("places_v24.sqlite");
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_bookmark_guid_annotation_removed()
{
  yield PlacesUtils.bookmarks.eraseEverything();

  let db = yield PlacesUtils.promiseDBConnection();
  let m = new Map([
    [PlacesUtils.placesRootId, PlacesUtils.bookmarks.rootGuid],
    [PlacesUtils.bookmarksMenuFolderId, PlacesUtils.bookmarks.menuGuid],
    [PlacesUtils.toolbarFolderId, PlacesUtils.bookmarks.toolbarGuid],
    [PlacesUtils.unfiledBookmarksFolderId, PlacesUtils.bookmarks.unfiledGuid],
    [PlacesUtils.tagsFolderId, PlacesUtils.bookmarks.tagsGuid]
  ]);

  let rows = yield db.execute(`SELECT id, guid FROM moz_bookmarks`);
  for (let row of rows) {
    let id = row.getResultByName("id");
    let guid = row.getResultByName("guid");
    Assert.equal(m.get(id), guid, "The root folder has the correct GUID");
  }
});
