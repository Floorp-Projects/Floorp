/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests migration invariants from schema version 10 to the current
 * schema version.
 */

////////////////////////////////////////////////////////////////////////////////
//// Constants

const kGuidAnnotationName = "sync/guid";
const kExpectedAnnotations = 5;
const kExpectedValidGuids = 2;

////////////////////////////////////////////////////////////////////////////////
//// Globals

// Set in test_initial_state to the value in the database.
var gItemGuid = [];
var gItemId = [];
var gPlaceGuid = [];
var gPlaceId = [];

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Determines if a guid is valid or not.
 *
 * @return true if it is a valid guid, false otherwise.
 */
function isValidGuid(aGuid)
{
  return /^[a-zA-Z0-9\-_]{12}$/.test(aGuid);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_initial_state()
{
  // Mostly sanity checks our starting DB to make sure it's setup as we expect
  // it to be.
  let dbFile = gProfD.clone();
  dbFile.append(kDBName);
  let db = Services.storage.openUnsharedDatabase(dbFile);

  let stmt = db.createStatement("PRAGMA journal_mode");
  do_check_true(stmt.executeStep());
  // WAL journal mode should not be set on this database.
  do_check_neq(stmt.getString(0).toLowerCase(), "wal");
  stmt.finalize();

  do_check_false(db.indexExists("moz_bookmarks_guid_uniqueindex"));
  do_check_false(db.indexExists("moz_places_guid_uniqueindex"));

  // There should be five item annotations for a bookmark guid.
  stmt = db.createStatement(
    "SELECT content AS guid, item_id "
  + "FROM moz_items_annos "
  + "WHERE anno_attribute_id = ( "
  +   "SELECT id "
  +   "FROM moz_anno_attributes "
  +   "WHERE name = :attr_name "
  + ") "
  );
  stmt.params.attr_name = kGuidAnnotationName;
  while (stmt.executeStep()) {
    gItemGuid.push(stmt.row.guid);
    gItemId.push(stmt.row.item_id)
  }
  do_check_eq(gItemGuid.length, gItemId.length);
  do_check_eq(gItemGuid.length, kExpectedAnnotations);
  stmt.finalize();

  // There should be five item annotations for a place guid.
  stmt = db.createStatement(
    "SELECT content AS guid, place_id "
  + "FROM moz_annos "
  + "WHERE anno_attribute_id = ( "
  +   "SELECT id "
  +   "FROM moz_anno_attributes "
  +   "WHERE name = :attr_name "
  + ") "
  );
  stmt.params.attr_name = kGuidAnnotationName;
  while (stmt.executeStep()) {
    gPlaceGuid.push(stmt.row.guid);
    gPlaceId.push(stmt.row.place_id)
  }
  do_check_eq(gPlaceGuid.length, gPlaceId.length);
  do_check_eq(gPlaceGuid.length, kExpectedAnnotations);
  stmt.finalize();

  // Check our schema version to make sure it is actually at 10.
  do_check_eq(db.schemaVersion, 10);

  db.close();
  run_next_test();
}

function test_moz_bookmarks_guid_exists()
{
  // This will throw if the column does not exist
  let stmt = DBConn().createStatement(
    "SELECT guid "
  + "FROM moz_bookmarks "
  );
  stmt.finalize();

  run_next_test();
}

function test_bookmark_guids_non_null()
{
  // First, sanity check that we have a non-zero amount of bookmarks.
  let stmt = DBConn().createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_bookmarks "
  );
  do_check_true(stmt.executeStep());
  do_check_neq(stmt.getInt32(0), 0);
  stmt.finalize();

  // Now, make sure we have no NULL guid entry.
  stmt = DBConn().createStatement(
    "SELECT guid "
  + "FROM moz_bookmarks "
  + "WHERE guid IS NULL "
  );
  do_check_false(stmt.executeStep());
  stmt.finalize();
  run_next_test();
}

function test_bookmark_guid_annotation_imported()
{
  // Make sure we have the imported guid; not a newly generated one.
  let stmt = DBConn().createStatement(
    "SELECT id "
  + "FROM moz_bookmarks "
  + "WHERE guid = :guid "
  + "AND id = :item_id "
  );
  let validGuids = 0;
  let seenGuids = [];
  for (let i = 0; i < gItemGuid.length; i++) {
    let guid = gItemGuid[i];
    stmt.params.guid = guid;
    stmt.params.item_id = gItemId[i];

    // Check that it is a valid guid that we expect, and that it is not a
    // duplicate (which would violate the unique constraint).
    let valid = isValidGuid(guid) && seenGuids.indexOf(guid) == -1;
    seenGuids.push(guid);

    if (valid) {
      validGuids++;
      do_check_true(stmt.executeStep());
    }
    else {
      do_check_false(stmt.executeStep());
    }
    stmt.reset();
  }
  do_check_eq(validGuids, kExpectedValidGuids);
  stmt.finalize();

  run_next_test();
}

function test_bookmark_guid_annotation_removed()
{
  let stmt = DBConn().createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_items_annos "
  + "WHERE anno_attribute_id = ( "
  +   "SELECT id "
  +   "FROM moz_anno_attributes "
  +   "WHERE name = :attr_name "
  + ") "
  );
  stmt.params.attr_name = kGuidAnnotationName;
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.getInt32(0), 0);
  stmt.finalize();

  run_next_test();
}

function test_moz_places_guid_exists()
{
  // This will throw if the column does not exist
  let stmt = DBConn().createStatement(
    "SELECT guid "
  + "FROM moz_places "
  );
  stmt.finalize();

  run_next_test();
}

function test_place_guids_non_null()
{
  // First, sanity check that we have a non-zero amount of places.
  let stmt = DBConn().createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_places "
  );
  do_check_true(stmt.executeStep());
  do_check_neq(stmt.getInt32(0), 0);
  stmt.finalize();

  // Now, make sure we have no NULL guid entry.
  stmt = DBConn().createStatement(
    "SELECT guid "
  + "FROM moz_places "
  + "WHERE guid IS NULL "
  );
  do_check_false(stmt.executeStep());
  stmt.finalize();
  run_next_test();
}

function test_place_guid_annotation_imported()
{
  // Make sure we have the imported guid; not a newly generated one.
  let stmt = DBConn().createStatement(
    "SELECT id "
  + "FROM moz_places "
  + "WHERE guid = :guid "
  + "AND id = :item_id "
  );
  let validGuids = 0;
  let seenGuids = [];
  for (let i = 0; i < gPlaceGuid.length; i++) {
    let guid = gPlaceGuid[i];
    stmt.params.guid = guid;
    stmt.params.item_id = gPlaceId[i];

    // Check that it is a valid guid that we expect, and that it is not a
    // duplicate (which would violate the unique constraint).
    let valid = isValidGuid(guid) && seenGuids.indexOf(guid) == -1;
    seenGuids.push(guid);

    if (valid) {
      validGuids++;
      do_check_true(stmt.executeStep());
    }
    else {
      do_check_false(stmt.executeStep());
    }
    stmt.reset();
  }
  do_check_eq(validGuids, kExpectedValidGuids);
  stmt.finalize();

  run_next_test();
}

function test_place_guid_annotation_removed()
{
  let stmt = DBConn().createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_annos "
  + "WHERE anno_attribute_id = ( "
  +   "SELECT id "
  +   "FROM moz_anno_attributes "
  +   "WHERE name = :attr_name "
  + ") "
  );
  stmt.params.attr_name = kGuidAnnotationName;
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.getInt32(0), 0);
  stmt.finalize();

  run_next_test();
}

function test_moz_hosts()
{
  // This will throw if the column does not exist
  let stmt = DBConn().createStatement(
    "SELECT host, frecency, typed, prefix "
  + "FROM moz_hosts "
  );
  stmt.finalize();

  // moz_hosts is populated asynchronously, so query asynchronously to serialize
  // to that.
  // check the number of entries in moz_hosts equals the number of
  // unique rev_host in moz_places
  stmt = DBConn().createAsyncStatement(
    "SELECT (SELECT COUNT(host) FROM moz_hosts), " +
           "(SELECT COUNT(DISTINCT rev_host) " +
            "FROM moz_places " +
            "WHERE LENGTH(rev_host) > 1)");
  try {
    stmt.executeAsync({
      handleResult: function (aResult) {
        this._hasResults = true;
        let row = aResult.getNextRow();
        let mozHostsCount = row.getResultByIndex(0);
        let mozPlacesCount = row.getResultByIndex(1);
        do_check_true(mozPlacesCount > 0);
        do_check_eq(mozPlacesCount, mozHostsCount);
      },
      handleError: function () {},
      handleCompletion: function (aReason) {
        do_check_eq(aReason, Ci.mozIStorageStatementCallback.REASON_FINISHED);
        do_check_true(this._hasResults);
        run_next_test();
      }
    });
  }
  finally {
    stmt.finalize();
  }
}

function test_final_state()
{
  // We open a new database mostly so that we can check that the settings were
  // actually saved.
  let dbFile = gProfD.clone();
  dbFile.append(kDBName);
  let db = Services.storage.openUnsharedDatabase(dbFile);

  let (stmt = db.createStatement("PRAGMA journal_mode")) {
    do_check_true(stmt.executeStep());
    // WAL journal mode should be set on this database.
    do_check_eq(stmt.getString(0).toLowerCase(), "wal");
    stmt.finalize();
  }

  do_check_true(db.indexExists("moz_bookmarks_guid_uniqueindex"));
  do_check_true(db.indexExists("moz_places_guid_uniqueindex"));
  do_check_true(db.indexExists("moz_favicons_guid_uniqueindex"));

  do_check_eq(db.schemaVersion, CURRENT_SCHEMA_VERSION);

  db.close();
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_initial_state,
  test_moz_bookmarks_guid_exists,
  test_bookmark_guids_non_null,
  test_bookmark_guid_annotation_imported,
  test_bookmark_guid_annotation_removed,
  test_moz_places_guid_exists,
  test_place_guids_non_null,
  test_place_guid_annotation_imported,
  test_place_guid_annotation_removed,
  test_moz_hosts,
  test_final_state,
].forEach(add_test);

function run_test()
{
  setPlacesDatabase("places_v10.sqlite");
  run_next_test();
}
