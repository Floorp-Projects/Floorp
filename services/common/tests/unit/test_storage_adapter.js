/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {Sqlite} = ChromeUtils.import("resource://gre/modules/Sqlite.jsm");
const {FirefoxAdapter} = ChromeUtils.import("resource://services-common/kinto-storage-adapter.js");

// set up what we need to make storage adapters
const kintoFilename = "kinto.sqlite";

function do_get_kinto_connection() {
  return FirefoxAdapter.openConnection({path: kintoFilename});
}

function do_get_kinto_adapter(sqliteHandle) {
  return new FirefoxAdapter("test", {sqliteHandle});
}

function do_get_kinto_db() {
  let profile = do_get_profile();
  let kintoDB = profile.clone();
  kintoDB.append(kintoFilename);
  return kintoDB;
}

function cleanup_kinto() {
  add_test(function cleanup_kinto_files() {
    let kintoDB = do_get_kinto_db();
    // clean up the db
    kintoDB.remove(false);
    run_next_test();
  });
}

function test_collection_operations() {
  add_task(async function test_kinto_clear() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.clear();
    await sqliteHandle.close();
  });

  // test creating new records... and getting them again
  add_task(async function test_kinto_create_new_get_existing() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    let record = {id: "test-id", foo: "bar"};
    await adapter.execute((transaction) => transaction.create(record));
    let newRecord = await adapter.get("test-id");
    // ensure the record is the same as when it was added
    deepEqual(record, newRecord);
    await sqliteHandle.close();
  });

  // test removing records
  add_task(async function test_kinto_can_remove_some_records() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    // create a second record
    let record = {id: "test-id-2", foo: "baz"};
    await adapter.execute((transaction) => transaction.create(record));
    let newRecord = await adapter.get("test-id-2");
    deepEqual(record, newRecord);
    // delete the record
    await adapter.execute((transaction) => transaction.delete(record.id));
    newRecord = await adapter.get(record.id);
    // ... and ensure it's no longer there
    Assert.equal(newRecord, undefined);
    // ensure the other record still exists
    newRecord = await adapter.get("test-id");
    Assert.notEqual(newRecord, undefined);
    await sqliteHandle.close();
  });

  // test getting records that don't exist
  add_task(async function test_kinto_get_non_existant() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    // Kinto expects adapters to either:
    let newRecord = await adapter.get("missing-test-id");
    // resolve with an undefined record
    Assert.equal(newRecord, undefined);
    await sqliteHandle.close();
  });

  // test updating records... and getting them again
  add_task(async function test_kinto_update_get_existing() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    let originalRecord = {id: "test-id", foo: "bar"};
    let updatedRecord = {id: "test-id", foo: "baz"};
    await adapter.clear();
    await adapter.execute((transaction) => transaction.create(originalRecord));
    await adapter.execute((transaction) => transaction.update(updatedRecord));
    // ensure the record exists
    let newRecord = await adapter.get("test-id");
    // ensure the record is the same as when it was added
    deepEqual(updatedRecord, newRecord);
    await sqliteHandle.close();
  });

  // test listing records
  add_task(async function test_kinto_list() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    let originalRecord = {id: "test-id-1", foo: "bar"};
    let records = await adapter.list();
    Assert.equal(records.length, 1);
    await adapter.execute((transaction) => transaction.create(originalRecord));
    records = await adapter.list();
    Assert.equal(records.length, 2);
    await sqliteHandle.close();
  });

  // test aborting transaction
  add_task(async function test_kinto_aborting_transaction() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.clear();
    let record = {id: 1, foo: "bar"};
    let error = null;
    try {
      await adapter.execute((transaction) => {
        transaction.create(record);
        throw new Error("unexpected");
      });
    } catch (e) {
      error = e;
    }
    Assert.notEqual(error, null);
    let records = await adapter.list();
    Assert.equal(records.length, 0);
    await sqliteHandle.close();
  });

  // test save and get last modified
  add_task(async function test_kinto_last_modified() {
    const initialValue = 0;
    const intendedValue = 12345678;

    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    let lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, initialValue);
    let result = await adapter.saveLastModified(intendedValue);
    Assert.equal(result, intendedValue);
    lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, intendedValue);

    // test saveLastModified parses values correctly
    result = await adapter.saveLastModified(" " + intendedValue + " blah");
    // should resolve with the parsed int
    Assert.equal(result, intendedValue);
    // and should have saved correctly
    lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, intendedValue);
    await sqliteHandle.close();
  });

  // test loadDump(records)
  add_task(async function test_kinto_import_records() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    let record1 = {id: 1, foo: "bar"};
    let record2 = {id: 2, foo: "baz"};
    let impactedRecords = await adapter.loadDump([
      record1, record2,
    ]);
    Assert.equal(impactedRecords.length, 2);
    let newRecord1 = await adapter.get("1");
    // ensure the record is the same as when it was added
    deepEqual(record1, newRecord1);
    let newRecord2 = await adapter.get("2");
    // ensure the record is the same as when it was added
    deepEqual(record2, newRecord2);
    await sqliteHandle.close();
  });

  add_task(async function test_kinto_import_records_should_override_existing() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.clear();
    let records = await adapter.list();
    Assert.equal(records.length, 0);
    let impactedRecords = await adapter.loadDump([
      {id: 1, foo: "bar"},
      {id: 2, foo: "baz"},
    ]);
    Assert.equal(impactedRecords.length, 2);
    await adapter.loadDump([
      {id: 1, foo: "baz"},
      {id: 3, foo: "bab"},
    ]);
    records = await adapter.list();
    Assert.equal(records.length, 3);
    let newRecord1 = await adapter.get("1");
    deepEqual(newRecord1.foo, "baz");
    await sqliteHandle.close();
  });

  add_task(async function test_import_updates_lastModified() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.loadDump([
      {id: 1, foo: "bar", last_modified: 1457896541},
      {id: 2, foo: "baz", last_modified: 1458796542},
    ]);
    let lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, 1458796542);
    await sqliteHandle.close();
  });

  add_task(async function test_import_preserves_older_lastModified() {
    let sqliteHandle = await do_get_kinto_connection();
    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.saveLastModified(1458796543);

    await adapter.loadDump([
      {id: 1, foo: "bar", last_modified: 1457896541},
      {id: 2, foo: "baz", last_modified: 1458796542},
    ]);
    let lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, 1458796543);
    await sqliteHandle.close();
  });

  add_task(async function test_save_metadata_preserves_lastModified() {
    let sqliteHandle = await do_get_kinto_connection();

    let adapter = do_get_kinto_adapter(sqliteHandle);
    await adapter.saveLastModified(42);

    await adapter.saveMetadata({id: "col"});

    let lastModified = await adapter.getLastModified();
    Assert.equal(lastModified, 42);
    await sqliteHandle.close();
  });
}

// test kinto db setup and operations in various scenarios
// test from scratch - no current existing database
add_test(function test_db_creation() {
  add_test(function test_create_from_scratch() {
    // ensure the file does not exist in the profile
    let kintoDB = do_get_kinto_db();
    Assert.ok(!kintoDB.exists());
    run_next_test();
  });

  test_collection_operations();

  cleanup_kinto();
  run_next_test();
});

// this is the closest we can get to a schema version upgrade at v1 - test an
// existing database
add_test(function test_creation_from_empty_db() {
  add_test(function test_create_from_empty_db() {
    // place an empty kinto db file in the profile
    let profile = do_get_profile();

    let emptyDB = do_get_file("test_storage_adapter/empty.sqlite");
    emptyDB.copyTo(profile, kintoFilename);

    run_next_test();
  });

  test_collection_operations();

  cleanup_kinto();
  run_next_test();
});

// test schema version upgrade at v2
add_test(function test_migration_from_v1_to_v2() {
  add_test(function test_migrate_from_v1_to_v2() {
    // place an empty kinto db file in the profile
    let profile = do_get_profile();

    let v1DB = do_get_file("test_storage_adapter/v1.sqlite");
    v1DB.copyTo(profile, kintoFilename);

    run_next_test();
  });

  add_test(async function schema_is_update_from_1_to_2() {
    // The `v1.sqlite` has schema version 1.
    let sqliteHandle = await Sqlite.openConnection({ path: kintoFilename });
    Assert.equal(await sqliteHandle.getSchemaVersion(), 1);
    await sqliteHandle.close();

    // The `.openConnection()` migrates it to version 2.
    sqliteHandle = await FirefoxAdapter.openConnection({ path: kintoFilename });
    Assert.equal(await sqliteHandle.getSchemaVersion(), 2);
    await sqliteHandle.close();

    run_next_test();
  });

  test_collection_operations();

  cleanup_kinto();
  run_next_test();
});
