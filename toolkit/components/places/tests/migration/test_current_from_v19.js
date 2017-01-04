/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ANNO_LEGACYGUID = "placesInternal/GUID";

var getTotalGuidAnnotationsCount = Task.async(function* (db) {
  let rows = yield db.execute(
    `SELECT count(*)
     FROM moz_items_annos a
     JOIN moz_anno_attributes b ON a.anno_attribute_id = b.id
     WHERE b.name = :attr_name
    `, { attr_name: ANNO_LEGACYGUID });
  return rows[0].getResultByIndex(0);
});

add_task(function* setup() {
  yield setupPlacesDatabase("places_v19.sqlite");
});

add_task(function* initial_state() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = yield Sqlite.openConnection({ path });

  Assert.equal((yield getTotalGuidAnnotationsCount(db)), 1,
               "There should be 1 obsolete guid annotation");
  yield db.close();
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_bookmark_guid_annotation_removed() {
  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield getTotalGuidAnnotationsCount(db)), 0,
               "There should be no more obsolete GUID annotations.");
});
