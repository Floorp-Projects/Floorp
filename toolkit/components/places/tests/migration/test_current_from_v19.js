/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests migration invariants from schema version 19 to the current
 * schema version.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const kGuidAnnotationName = "placesInternal/GUID";

function getTotalGuidAnnotationsCount(aStorageConnection) {
  stmt = aStorageConnection.createStatement(
    "SELECT count(*) "
  + "FROM moz_items_annos a "
  + "JOIN moz_anno_attributes b ON a.anno_attribute_id = b.id "
  + "WHERE b.name = :attr_name"
  );
  try {
    stmt.params.attr_name = kGuidAnnotationName;
    do_check_true(stmt.executeStep());
    return stmt.getInt32(0);
  } finally {
    stmt.finalize();
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

function run_test()
{
  setPlacesDatabase("places_v19.sqlite");
  run_next_test();
}

add_test(function test_initial_state()
{
  let dbFile = gProfD.clone();
  dbFile.append(kDBName);
  let db = Services.storage.openUnsharedDatabase(dbFile);

  // There should be an obsolete bookmark GUID annotation.
  do_check_eq(getTotalGuidAnnotationsCount(db), 1);

  // Check our schema version to make sure it is actually at 19.
  do_check_eq(db.schemaVersion, 19);

  db.close();
  run_next_test();
});

add_test(function test_bookmark_guid_annotation_removed()
{

  // There should be no obsolete bookmark GUID annotation anymore.
  do_check_eq(getTotalGuidAnnotationsCount(DBConn()), 0);

  run_next_test();
});
