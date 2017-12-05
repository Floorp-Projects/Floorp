/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageConnection

// Test Functions

add_task(async function test_connectionReady_open() {
  // there doesn't seem to be a way for the connection to not be ready (unless
  // we close it with mozIStorageConnection::Close(), but we don't for this).
  // It can only fail if GetPath fails on the database file, or if we run out
  // of memory trying to use an in-memory database

  var msc = getOpenedDatabase();
  do_check_true(msc.connectionReady);
});

add_task(async function test_connectionReady_closed() {
  // This also tests mozIStorageConnection::Close()

  var msc = getOpenedDatabase();
  msc.close();
  do_check_false(msc.connectionReady);
  gDBConn = null; // this is so later tests don't start to fail.
});

add_task(async function test_databaseFile() {
  var msc = getOpenedDatabase();
  do_check_true(getTestDB().equals(msc.databaseFile));
});

add_task(async function test_tableExists_not_created() {
  var msc = getOpenedDatabase();
  do_check_false(msc.tableExists("foo"));
});

add_task(async function test_indexExists_not_created() {
  var msc = getOpenedDatabase();
  do_check_false(msc.indexExists("foo"));
});

add_task(async function test_temp_tableExists_and_indexExists() {
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("CREATE TEMP TABLE test_temp(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)");
  do_check_true(msc.tableExists("test_temp"));

  msc.executeSimpleSQL("CREATE INDEX test_temp_ind ON test_temp (name)");
  do_check_true(msc.indexExists("test_temp_ind"));

  msc.executeSimpleSQL("DROP INDEX test_temp_ind");
  msc.executeSimpleSQL("DROP TABLE test_temp");
});

add_task(async function test_createTable_not_created() {
  var msc = getOpenedDatabase();
  msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
  do_check_true(msc.tableExists("test"));
});

add_task(async function test_indexExists_created() {
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("CREATE INDEX name_ind ON test (name)");
  do_check_true(msc.indexExists("name_ind"));
});

add_task(async function test_createTable_already_created() {
  var msc = getOpenedDatabase();
  do_check_true(msc.tableExists("test"));
  Assert.throws(() => msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT"),
                /NS_ERROR_FAILURE/);
});

add_task(async function test_attach_createTable_tableExists_indexExists() {
  var msc = getOpenedDatabase();
  var file = do_get_file("storage_attach.sqlite", true);
  var msc2 = getDatabase(file);
  msc.executeSimpleSQL("ATTACH DATABASE '" + file.path + "' AS sample");

  do_check_false(msc.tableExists("sample.test"));
  msc.createTable("sample.test", "id INTEGER PRIMARY KEY, name TEXT");
  do_check_true(msc.tableExists("sample.test"));
  Assert.throws(() => msc.createTable("sample.test", "id INTEGER PRIMARY KEY, name TEXT"),
                /NS_ERROR_FAILURE/);

  do_check_false(msc.indexExists("sample.test_ind"));
  msc.executeSimpleSQL("CREATE INDEX sample.test_ind ON test (name)");
  do_check_true(msc.indexExists("sample.test_ind"));

  msc.executeSimpleSQL("DETACH DATABASE sample");
  msc2.close();
  try {
    file.remove(false);
  } catch (e) {
    // Do nothing.
  }
});

add_task(async function test_lastInsertRowID() {
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("INSERT INTO test (name) VALUES ('foo')");
  do_check_eq(1, msc.lastInsertRowID);
});

add_task(async function test_transactionInProgress_no() {
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
});

add_task(async function test_transactionInProgress_yes() {
  var msc = getOpenedDatabase();
  msc.beginTransaction();
  do_check_true(msc.transactionInProgress);
  msc.commitTransaction();
  do_check_false(msc.transactionInProgress);

  msc.beginTransaction();
  do_check_true(msc.transactionInProgress);
  msc.rollbackTransaction();
  do_check_false(msc.transactionInProgress);
});

add_task(async function test_commitTransaction_no_transaction() {
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  Assert.throws(() => msc.commitTransaction(), /NS_ERROR_UNEXPECTED/);
});

add_task(async function test_rollbackTransaction_no_transaction() {
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  Assert.throws(() => msc.rollbackTransaction(), /NS_ERROR_UNEXPECTED/);
});

add_task(async function test_get_schemaVersion_not_set() {
  do_check_eq(0, getOpenedDatabase().schemaVersion);
});

add_task(async function test_set_schemaVersion() {
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
});

add_task(async function test_set_schemaVersion_same() {
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version; // should still work ok
  do_check_eq(version, msc.schemaVersion);
});

add_task(async function test_set_schemaVersion_negative() {
  var msc = getOpenedDatabase();
  const version = -1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
});

add_task(async function test_createTable() {
  var temp = getTestDB().parent;
  temp.append("test_db_table");
  try {
    var con = getService().openDatabase(temp);
    con.createTable("a", "");
  } catch (e) {
    if (temp.exists()) {
      try {
        temp.remove(false);
      } catch (e2) {
        // Do nothing.
      }
    }
    do_check_true(e.result == Cr.NS_ERROR_NOT_INITIALIZED ||
                  e.result == Cr.NS_ERROR_FAILURE);
  } finally {
    if (con) {
      con.close();
    }
  }
});

add_task(async function test_defaultSynchronousAtNormal() {
  getOpenedDatabase();
  var stmt = createStatement("PRAGMA synchronous;");
  try {
    stmt.executeStep();
    do_check_eq(1, stmt.getInt32(0));
  } finally {
    stmt.reset();
    stmt.finalize();
  }
});

// must be ran before executeAsync tests
add_task(async function test_close_does_not_spin_event_loop() {
  // We want to make sure that the event loop on the calling thread does not
  // spin when close is called.
  let event = {
    ran: false,
    run() {
      this.ran = true;
    },
  };

  // Post the event before we call close, so it would run if the event loop was
  // spun during close.
  Cc["@mozilla.org/thread-manager;1"].
    getService(Ci.nsIThreadManager).dispatchToMainThread(event);

  // Sanity check, then close the database.  Afterwards, we should not have ran!
  do_check_false(event.ran);
  getOpenedDatabase().close();
  do_check_false(event.ran);

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
});

add_task(async function test_asyncClose_succeeds_with_finalized_async_statement() {
  // XXX this test isn't perfect since we can't totally control when events will
  //     run.  If this paticular function fails randomly, it means we have a
  //     real bug.

  // We want to make sure we create a cached async statement to make sure that
  // when we finalize our statement, we end up finalizing the async one too so
  // close will succeed.
  let stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync();
  stmt.finalize();

  await asyncClose(getOpenedDatabase());
  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
});

// Would assert on debug builds.
if (!AppConstants.DEBUG) {
  add_task(async function test_close_then_release_statement() {
    // Testing the behavior in presence of a bad client that finalizes
    // statements after the database has been closed (typically by
    // letting the gc finalize the statement).
    let db = getOpenedDatabase();
    let stmt = createStatement("SELECT * FROM test -- test_close_then_release_statement");
    db.close();
    stmt.finalize(); // Finalize too late - this should not crash

    // Reset gDBConn so that later tests will get a new connection object.
    gDBConn = null;
  });

  add_task(async function test_asyncClose_then_release_statement() {
    // Testing the behavior in presence of a bad client that finalizes
    // statements after the database has been async closed (typically by
    // letting the gc finalize the statement).
    let db = getOpenedDatabase();
    let stmt = createStatement("SELECT * FROM test -- test_asyncClose_then_release_statement");
    await asyncClose(db);
    stmt.finalize(); // Finalize too late - this should not crash

    // Reset gDBConn so that later tests will get a new connection object.
    gDBConn = null;
  });
}

// In debug builds this would cause a fatal assertion.
if (!AppConstants.DEBUG) {
  add_task(async function test_close_fails_with_async_statement_ran() {
    let stmt = createStatement("SELECT * FROM test");
    stmt.executeAsync();
    stmt.finalize();

    let db = getOpenedDatabase();
    Assert.throws(() => db.close(), /NS_ERROR_UNEXPECTED/);
    // Reset gDBConn so that later tests will get a new connection object.
    gDBConn = null;
  });
}

add_task(async function test_clone_optional_param() {
  let db1 = getService().openUnsharedDatabase(getTestDB());
  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // A write statement should not fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "dwitte";
  stmt.execute();
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  // Additionally check that it is a connection on the same database.
  do_check_true(db1.databaseFile.equals(db2.databaseFile));

  db1.close();
  db2.close();
});

async function standardAsyncTest(promisedDB, name, shouldInit = false) {
  do_print("Performing standard async test " + name);

  let adb = await promisedDB;
  do_check_true(adb instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(adb instanceof Ci.mozIStorageConnection);

  if (shouldInit) {
    let stmt = adb.createAsyncStatement("CREATE TABLE test(name TEXT)");
    await executeAsync(stmt);
    stmt.finalize();
  }

  // Generate a name to insert and fetch back
  name = "worker bee " + Math.random() + " (" + name + ")";

  let stmt = adb.createAsyncStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = name;
  let result = await executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data");
  stmt = adb.createAsyncStatement("SELECT * FROM test");
  let found = false;
  await executeAsync(stmt, function(results) {
    do_print("Data has been extracted");
    for (let row = results.getNextRow(); row != null; row = results.getNextRow()) {
      if (row.getResultByName("name") == name) {
        found = true;
        break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  await asyncClose(adb);

  do_print("Standard async test " + name + " complete");
}

add_task(async function test_open_async() {
  await standardAsyncTest(openAsyncDatabase(getTestDB(), null), "default");
  await standardAsyncTest(openAsyncDatabase(getTestDB()), "no optional arg");
  await standardAsyncTest(openAsyncDatabase(getTestDB(),
    {shared: false, growthIncrement: 54}), "non-default options");
  await standardAsyncTest(openAsyncDatabase("memory"),
    "in-memory database", true);
  await standardAsyncTest(openAsyncDatabase("memory",
    {shared: false}),
    "in-memory database and options", true);

  do_print("Testing async opening with bogus options 0");
  let raised = false;
  let adb = null;

  try {
    adb = await openAsyncDatabase("memory", {shared: false, growthIncrement: 54});
  } catch (ex) {
    raised = true;
  } finally {
    if (adb) {
      await asyncClose(adb);
    }
  }
  do_check_true(raised);

  do_print("Testing async opening with bogus options 1");
  raised = false;
  adb = null;
  try {
    adb = await openAsyncDatabase(getTestDB(), {shared: "forty-two"});
  } catch (ex) {
    raised = true;
  } finally {
    if (adb) {
      await asyncClose(adb);
    }
  }
  do_check_true(raised);

  do_print("Testing async opening with bogus options 2");
  raised = false;
  adb = null;
  try {
    adb = await openAsyncDatabase(getTestDB(), {growthIncrement: "forty-two"});
  } catch (ex) {
    raised = true;
  } finally {
    if (adb) {
      await asyncClose(adb);
    }
  }
  do_check_true(raised);
});


add_task(async function test_async_open_with_shared_cache() {
  do_print("Testing that opening with a shared cache doesn't break stuff");
  let adb = await openAsyncDatabase(getTestDB(), {shared: true});

  let stmt = adb.createAsyncStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "clockworker";
  let result = await executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data");
  stmt = adb.createAsyncStatement("SELECT * FROM test");
  let found = false;
  await executeAsync(stmt, function(results) {
    do_print("Data has been extracted");
    for (let row = results.getNextRow(); row != null; row = results.getNextRow()) {
      if (row.getResultByName("name") == "clockworker") {
        found = true;
        break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  await asyncClose(adb);
});

add_task(async function test_clone_trivial_async() {
  do_print("Open connection");
  let db = getService().openDatabase(getTestDB());
  do_check_true(db instanceof Ci.mozIStorageAsyncConnection);
  do_print("AsyncClone connection");
  let clone = await asyncClone(db, true);
  do_check_true(clone instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(clone instanceof Ci.mozIStorageConnection);
  do_print("Close connection");
  await asyncClose(db);
  do_print("Close clone");
  await asyncClose(clone);
});

add_task(async function test_clone_no_optional_param_async() {
  "use strict";
  do_print("Testing async cloning");
  let adb1 = await openAsyncDatabase(getTestDB(), null);
  do_check_true(adb1 instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(adb1 instanceof Ci.mozIStorageConnection);

  do_print("Cloning database");

  let adb2 = await asyncClone(adb1);
  do_print("Testing that the cloned db is a mozIStorageAsyncConnection " +
           "and not a mozIStorageConnection");
  do_check_true(adb2 instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(adb2 instanceof Ci.mozIStorageConnection);

  do_print("Inserting data into source db");
  let stmt = adb1.
               createAsyncStatement("INSERT INTO test (name) VALUES (:name)");

  stmt.params.name = "yoric";
  let result = await executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data from clone db");
  stmt = adb2.createAsyncStatement("SELECT * FROM test");
  let found = false;
  await executeAsync(stmt, function(results) {
    do_print("Data has been extracted");
    for (let row = results.getNextRow(); row != null; row = results.getNextRow()) {
      if (row.getResultByName("name") == "yoric") {
        found = true;
        break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  do_print("Closing databases");
  await asyncClose(adb2);
  do_print("First db closed");

  await asyncClose(adb1);
  do_print("Second db closed");
});

add_task(async function test_clone_readonly() {
  let db1 = getService().openUnsharedDatabase(getTestDB());
  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // A write statement should fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "reed";
  expectError(Cr.NS_ERROR_FILE_READ_ONLY, () => stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  db1.close();
  db2.close();
});

add_task(async function test_clone_shared_readonly() {
  let db1 = getService().openDatabase(getTestDB());
  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "parker";
  // TODO currently SQLite does not actually work correctly here.  The behavior
  //      we want is commented out, and the current behavior is being tested
  //      for.  Our IDL comments will have to be updated when this starts to
  //      work again.
  stmt.execute();
  // expectError(Components.results.NS_ERROR_FILE_READ_ONLY, () => stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  db1.close();
  db2.close();
});

add_task(async function test_close_clone_fails() {
  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  calls.forEach(function(methodName) {
    let db = getService()[methodName](getTestDB());
    db.close();
    expectError(Cr.NS_ERROR_NOT_INITIALIZED, () => db.clone());
  });
});

add_task(async function test_memory_clone_fails() {
  let db = getService().openSpecialDatabase("memory");
  db.close();
  expectError(Cr.NS_ERROR_NOT_INITIALIZED, () => db.clone());
});

add_task(async function test_clone_copies_functions() {
  const FUNC_NAME = "test_func";
  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  let functionMethods = [
    "createFunction",
    "createAggregateFunction",
  ];
  calls.forEach(function(methodName) {
    [true, false].forEach(function(readOnly) {
      functionMethods.forEach(function(functionMethod) {
        let db1 = getService()[methodName](getTestDB());
        // Create a function for db1.
        db1[functionMethod](FUNC_NAME, 1, {
          onFunctionCall: () => 0,
          onStep: () => 0,
          onFinal: () => 0,
        });

        // Clone it, and make sure the function exists still.
        let db2 = db1.clone(readOnly);
        // Note: this would fail if the function did not exist.
        let stmt = db2.createStatement("SELECT " + FUNC_NAME + "(id) FROM test");
        stmt.finalize();
        db1.close();
        db2.close();
      });
    });
  });
});

add_task(async function test_clone_copies_overridden_functions() {
  const FUNC_NAME = "lower";
  function test_func() {
    this.called = false;
  }
  test_func.prototype = {
    onFunctionCall() {
      this.called = true;
    },
    onStep() {
      this.called = true;
    },
    onFinal: () => 0,
  };

  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  let functionMethods = [
    "createFunction",
    "createAggregateFunction",
  ];
  calls.forEach(function(methodName) {
    [true, false].forEach(function(readOnly) {
      functionMethods.forEach(function(functionMethod) {
        let db1 = getService()[methodName](getTestDB());
        // Create a function for db1.
        let func = new test_func();
        db1[functionMethod](FUNC_NAME, 1, func);
        do_check_false(func.called);

        // Clone it, and make sure the function gets called.
        let db2 = db1.clone(readOnly);
        let stmt = db2.createStatement("SELECT " + FUNC_NAME + "(id) FROM test");
        stmt.executeStep();
        do_check_true(func.called);
        stmt.finalize();
        db1.close();
        db2.close();
      });
    });
  });
});

add_task(async function test_clone_copies_pragmas() {
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: true },
    { name: "journal_size_limit", value: 524288, copied: true },
    { name: "synchronous", value: 2, copied: true },
    { name: "wal_autocheckpoint", value: 16, copied: true },
    { name: "busy_timeout", value: 50, copied: true },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function(pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function(pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function(pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  db1.close();
  db2.close();
});

add_task(async function test_readonly_clone_copies_pragmas() {
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: false },
    { name: "journal_size_limit", value: 524288, copied: false },
    { name: "synchronous", value: 2, copied: false },
    { name: "wal_autocheckpoint", value: 16, copied: false },
    { name: "busy_timeout", value: 50, copied: false },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function(pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function(pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function(pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  db1.close();
  db2.close();
});

add_task(async function test_clone_attach_database() {
  let db1 = getService().openUnsharedDatabase(getTestDB());

  let c = 0;
  function attachDB(conn, name) {
    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("test_storage_" + (++c) + ".sqlite");
    let db = getService().openUnsharedDatabase(file);
    conn.executeSimpleSQL(`ATTACH DATABASE '${db.databaseFile.path}' AS ${name}`);
    db.close();
  }
  attachDB(db1, "attached_1");
  attachDB(db1, "attached_2");

  // These should not throw.
  let stmt = db1.createStatement("SELECT * FROM attached_1.sqlite_master");
  stmt.finalize();
  stmt = db1.createStatement("SELECT * FROM attached_2.sqlite_master");
  stmt.finalize();

  // R/W clone.
  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // These should not throw.
  stmt = db2.createStatement("SELECT * FROM attached_1.sqlite_master");
  stmt.finalize();
  stmt = db2.createStatement("SELECT * FROM attached_2.sqlite_master");
  stmt.finalize();

  // R/O clone.
  let db3 = db1.clone(true);
  do_check_true(db3.connectionReady);

  // These should not throw.
  stmt = db3.createStatement("SELECT * FROM attached_1.sqlite_master");
  stmt.finalize();
  stmt = db3.createStatement("SELECT * FROM attached_2.sqlite_master");
  stmt.finalize();

  db1.close();
  db2.close();
  db3.close();
});

add_task(async function test_async_clone_with_temp_trigger_and_table() {
  do_print("Open connection");
  let db = getService().openDatabase(getTestDB());
  do_check_true(db instanceof Ci.mozIStorageAsyncConnection);

  do_print("Set up tables on original connection");
  let createQueries = [
    `CREATE TEMP TABLE test_temp(name TEXT)`,
    `CREATE INDEX test_temp_idx ON test_temp(name)`,
    `CREATE TEMP TRIGGER test_temp_afterdelete_trigger
     AFTER DELETE ON test_temp FOR EACH ROW
     BEGIN
       INSERT INTO test(name) VALUES(OLD.name);
     END`];
  for (let query of createQueries) {
    let stmt = db.createAsyncStatement(query);
    await executeAsync(stmt);
    stmt.finalize();
  }

  do_print("Create read-write clone with temp tables");
  let readWriteClone = await asyncClone(db, false);
  do_check_true(readWriteClone instanceof Ci.mozIStorageAsyncConnection);

  do_print("Insert into temp table on read-write clone");
  let insertStmt = readWriteClone.createAsyncStatement(`
    INSERT INTO test_temp(name) VALUES('mak'), ('standard8'), ('markh')`);
  await executeAsync(insertStmt);
  insertStmt.finalize();

  do_print("Fire temp trigger on read-write clone");
  let deleteStmt = readWriteClone.createAsyncStatement(`
    DELETE FROM test_temp`);
  await executeAsync(deleteStmt);
  deleteStmt.finalize();

  do_print("Read from original connection");
  let names = [];
  let readStmt = db.createStatement(`SELECT name FROM test`);
  while (readStmt.executeStep()) {
    names.push(readStmt.getUTF8String(0));
  }
  readStmt.finalize();
  do_check_true(names.includes("mak"));
  do_check_true(names.includes("standard8"));
  do_check_true(names.includes("markh"));

  do_print("Create read-only clone");
  let readOnlyClone = await asyncClone(db, true);
  do_check_true(readOnlyClone instanceof Ci.mozIStorageAsyncConnection);

  do_print("Read-only clone shouldn't have temp entities");
  let badStmt = readOnlyClone.createAsyncStatement(`SELECT 1 FROM test_temp`);
  await Assert.rejects(executeAsync(badStmt));
  badStmt.finalize();

  do_print("Clean up");
  for (let conn of [db, readWriteClone, readOnlyClone]) {
    await asyncClose(conn);
  }
});

add_task(async function test_sync_clone_in_transaction() {
  do_print("Open connection");
  let db = getService().openDatabase(getTestDB());
  do_check_true(db instanceof Ci.mozIStorageAsyncConnection);

  do_print("Begin transaction on main connection");
  db.beginTransaction();

  do_print("Create temp table and trigger in transaction");
  let createQueries = [
    `CREATE TEMP TABLE test_temp(name TEXT)`,
    `CREATE TEMP TRIGGER test_temp_afterdelete_trigger
     AFTER DELETE ON test_temp FOR EACH ROW
     BEGIN
       INSERT INTO test(name) VALUES(OLD.name);
     END`,
  ];
  for (let query of createQueries) {
    db.executeSimpleSQL(query);
  }

  do_print("Clone main connection while transaction is in progress");
  let clone = db.clone(/* aReadOnly */ false);

  // Dropping the table also drops `test_temp_afterdelete_trigger`.
  do_print("Drop temp table on main connection");
  db.executeSimpleSQL(`DROP TABLE test_temp`);

  do_print("Commit transaction");
  db.commitTransaction();

  do_print("Clone connection should still have temp entities");
  let readTempStmt = clone.createStatement(`SELECT 1 FROM test_temp`);
  readTempStmt.execute();
  readTempStmt.finalize();

  do_print("Clean up");

  db.close();
  clone.close();
});

add_task(async function test_sync_clone_with_function() {
  do_print("Open connection");
  let db = getService().openDatabase(getTestDB());
  do_check_true(db instanceof Ci.mozIStorageAsyncConnection);

  do_print("Create SQL function");
  function storeLastInsertedNameFunc() {
    this.name = null;
  }
  storeLastInsertedNameFunc.prototype = {
    onFunctionCall(args) {
      this.name = args.getUTF8String(0);
    },
  };
  let func = new storeLastInsertedNameFunc();
  db.createFunction("store_last_inserted_name", 1, func);

  do_print("Create temp trigger on main connection");
  db.executeSimpleSQL(`
    CREATE TEMP TRIGGER test_afterinsert_trigger
    AFTER INSERT ON test FOR EACH ROW
    BEGIN
      SELECT store_last_inserted_name(NEW.name);
    END`);

  do_print("Clone main connection");
  let clone = db.clone(/* aReadOnly */ false);

  do_print("Write to clone");
  clone.executeSimpleSQL(`INSERT INTO test(name) VALUES('kit')`);

  do_check_eq(func.name, "kit");

  do_print("Clean up");
  db.close();
  clone.close();
});

add_task(async function test_getInterface() {
  let db = getOpenedDatabase();
  let target = db.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIEventTarget);
  // Just check that target is non-null.  Other tests will ensure that it has
  // the correct value.
  do_check_true(target != null);

  await asyncClose(db);
  gDBConn = null;
});
