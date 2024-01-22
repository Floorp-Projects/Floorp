/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageService except for
// openSpecialDatabase, which is tested by test_storage_service_special.js and
// openUnsharedDatabase, which is tested by test_storage_service_unshared.js.

function test_openDatabase_null_file() {
  try {
    Services.storage.openDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    Assert.equal(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openDatabase_file_DNE() {
  // the file should be created after calling

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(OPEN_HISTOGRAM);

  var db = getTestDB();
  Assert.ok(!db.exists());
  Services.storage.openDatabase(db);
  Assert.ok(db.exists());

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    db.leafName,
    TELEMETRY_VALUES.success,
    1
  );
}

function test_openDatabase_file_exists() {
  // it should already exist from our last test

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(OPEN_HISTOGRAM);

  var db = getTestDB();
  Assert.ok(db.exists());
  Services.storage.openDatabase(db);
  Assert.ok(db.exists());

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    db.leafName,
    TELEMETRY_VALUES.success,
    1
  );
}

function test_corrupt_db_throws_with_openDatabase() {
  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(OPEN_HISTOGRAM);

  let db = getCorruptDB();

  try {
    getDatabase(db);
    do_throw("should not be here");
  } catch (e) {
    Assert.equal(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    db.leafName,
    TELEMETRY_VALUES.corrupt,
    1
  );
}

function test_fake_db_throws_with_openDatabase() {
  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(OPEN_HISTOGRAM);

  let db = getFakeDB();

  try {
    getDatabase(db);
    do_throw("should not be here");
  } catch (e) {
    Assert.equal(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    db.leafName,
    TELEMETRY_VALUES.corrupt,
    1
  );
}

function test_openDatabase_directory() {
  let dir = getTestDB().parent;
  dir.append("test_storage_temp");
  if (dir.exists()) {
    dir.remove(true);
  }
  dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  Assert.ok(dir.exists());

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(OPEN_HISTOGRAM);

  try {
    getDatabase(dir);
    do_throw("should not be here");
  } catch (e) {
    Assert.equal(Cr.NS_ERROR_FILE_ACCESS_DENIED, e.result);
  }

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    dir.leafName,
    TELEMETRY_VALUES.access,
    1
  );

  dir.remove(true);
}

function test_read_gooddb() {
  let file = do_get_file("goodDB.sqlite");
  let db = getDatabase(file);

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(QUERY_HISTOGRAM);

  db.executeSimpleSQL("SELECT * FROM Foo;");

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.success,
    1
  );

  histogram.clear();

  let stmt = db.createStatement("SELECT id from Foo");

  while (true) {
    if (!stmt.executeStep()) {
      break;
    }
  }

  stmt.finalize();

  // A single statement should count as a single access.
  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.success,
    1
  );

  histogram.clear();

  Assert.throws(
    () => db.executeSimpleSQL("INSERT INTO Foo (rowid) VALUES ('test');"),
    /NS_ERROR_FAILURE/,
    "Executing sql should fail."
  );
  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.misuse,
    1
  );
}

function test_read_baddb() {
  let file = do_get_file("baddataDB.sqlite");
  let db = getDatabase(file);

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(QUERY_HISTOGRAM);

  Assert.throws(
    () => db.executeSimpleSQL("SELECT * FROM Foo"),
    /NS_ERROR_FILE_CORRUPTED/,
    "Executing sql should fail."
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.corrupt,
    1
  );

  histogram.clear();

  let stmt = db.createStatement("SELECT * FROM Foo");
  Assert.throws(
    () => stmt.executeStep(),
    /NS_ERROR_FILE_CORRUPTED/,
    "Executing a statement should fail."
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.corrupt,
    1
  );
}

function test_busy_telemetry() {
  // Thunderbird doesn't have one or more of the probes used in this test.
  // Ensure the data is collected anyway.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  let file = do_get_file("goodDB.sqlite");
  let conn1 = Services.storage.openUnsharedDatabase(file);
  let conn2 = Services.storage.openUnsharedDatabase(file);

  conn1.beginTransaction();
  conn1.executeSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)");

  let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(QUERY_HISTOGRAM);
  Assert.throws(
    () =>
      conn2.executeSimpleSQL("CREATE TABLE test_busy (id INTEGER PRIMARY KEY)"),
    /NS_ERROR_STORAGE_BUSY/,
    "Nested transaction on second connection should fail"
  );
  TelemetryTestUtils.assertKeyedHistogramValue(
    histogram,
    file.leafName,
    TELEMETRY_VALUES.busy,
    1
  );

  conn1.rollbackTransaction();
}

var tests = [
  test_openDatabase_null_file,
  test_openDatabase_file_DNE,
  test_openDatabase_file_exists,
  test_corrupt_db_throws_with_openDatabase,
  test_fake_db_throws_with_openDatabase,
  test_openDatabase_directory,
  test_read_gooddb,
  test_read_baddb,
  test_busy_telemetry,
];

function run_test() {
  // Thunderbird doesn't have one or more of the probes used in this test.
  // Ensure the data is collected anyway.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
