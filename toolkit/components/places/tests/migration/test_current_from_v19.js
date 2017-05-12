/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ANNO_LEGACYGUID = "placesInternal/GUID";

var getTotalGuidAnnotationsCount = async function(db) {
  let rows = await db.execute(
    `SELECT count(*)
     FROM moz_items_annos a
     JOIN moz_anno_attributes b ON a.anno_attribute_id = b.id
     WHERE b.name = :attr_name
    `, { attr_name: ANNO_LEGACYGUID });
  return rows[0].getResultByIndex(0);
};

add_task(async function setup() {
  await setupPlacesDatabase("places_v19.sqlite");
});

add_task(async function initial_state() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  Assert.equal((await getTotalGuidAnnotationsCount(db)), 1,
               "There should be 1 obsolete guid annotation");
  await db.close();
});

add_task(async function database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_bookmark_guid_annotation_removed() {
  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await getTotalGuidAnnotationsCount(db)), 0,
               "There should be no more obsolete GUID annotations.");
});
