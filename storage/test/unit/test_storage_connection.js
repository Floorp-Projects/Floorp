/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageConnection

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function asyncClone(db, readOnly) {
  let deferred = Promise.defer();
  db.asyncClone(readOnly, function(status, db2) {
    if (Components.isSuccessCode(status)) {
      deferred.resolve(db2);
    } else {
      deferred.reject(status);
    }
  });
  return deferred.promise;
}

function asyncClose(db) {
  let deferred = Promise.defer();
  db.asyncClose(function(status) {
    if (Components.isSuccessCode(status)) {
      deferred.resolve();
    } else {
      deferred.reject(status);
    }
  });
  return deferred.promise;
}

function openAsyncDatabase(file, options) {
  let deferred = Promise.defer();
  let properties;
  if (options) {
    properties = Cc["@mozilla.org/hash-property-bag;1"].
        createInstance(Ci.nsIWritablePropertyBag);
    for (let k in options) {
      properties.setProperty(k, options[k]);
    }
  }
  getService().openAsyncDatabase(file, properties, function(status, db) {
    if (Components.isSuccessCode(status)) {
      deferred.resolve(db.QueryInterface(Ci.mozIStorageAsyncConnection));
    } else {
      deferred.reject(status);
    }
  });
  return deferred.promise;
}

function executeAsync(statement, onResult) {
  let deferred = Promise.defer();
  statement.executeAsync({
    handleError: function(error) {
      deferred.reject(error);
    },
    handleResult: function(result) {
      if (onResult) {
        onResult(result);
      }
    },
    handleCompletion: function(result) {
      deferred.resolve(result);
    }
  });
  return deferred.promise;
}

add_task(function test_connectionReady_open()
{
  // there doesn't seem to be a way for the connection to not be ready (unless
  // we close it with mozIStorageConnection::Close(), but we don't for this).
  // It can only fail if GetPath fails on the database file, or if we run out
  // of memory trying to use an in-memory database

  var msc = getOpenedDatabase();
  do_check_true(msc.connectionReady);
});

add_task(function test_connectionReady_closed()
{
  // This also tests mozIStorageConnection::Close()

  var msc = getOpenedDatabase();
  msc.close();
  do_check_false(msc.connectionReady);
  gDBConn = null; // this is so later tests don't start to fail.
});

add_task(function test_databaseFile()
{
  var msc = getOpenedDatabase();
  do_check_true(getTestDB().equals(msc.databaseFile));
});

add_task(function test_tableExists_not_created()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.tableExists("foo"));
});

add_task(function test_indexExists_not_created()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.indexExists("foo"));
});

add_task(function test_createTable_not_created()
{
  var msc = getOpenedDatabase();
  msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
  do_check_true(msc.tableExists("test"));
});

add_task(function test_indexExists_created()
{
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("CREATE INDEX name_ind ON test (name)");
  do_check_true(msc.indexExists("name_ind"));
});

add_task(function test_createTable_already_created()
{
  var msc = getOpenedDatabase();
  do_check_true(msc.tableExists("test"));
  try {
    msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
});

add_task(function test_lastInsertRowID()
{
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("INSERT INTO test (name) VALUES ('foo')");
  do_check_eq(1, msc.lastInsertRowID);
});

add_task(function test_transactionInProgress_no()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
});

add_task(function test_transactionInProgress_yes()
{
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

add_task(function test_commitTransaction_no_transaction()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  try {
    msc.commitTransaction();
    do_throw("We should not get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, e.result);
  }
});

add_task(function test_rollbackTransaction_no_transaction()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  try {
    msc.rollbackTransaction();
    do_throw("We should not get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, e.result);
  }
});

add_task(function test_get_schemaVersion_not_set()
{
  do_check_eq(0, getOpenedDatabase().schemaVersion);
});

add_task(function test_set_schemaVersion()
{
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
});

add_task(function test_set_schemaVersion_same()
{
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version; // should still work ok
  do_check_eq(version, msc.schemaVersion);
});

add_task(function test_set_schemaVersion_negative()
{
  var msc = getOpenedDatabase();
  const version = -1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
});

add_task(function test_createTable(){
  var temp = getTestDB().parent;
  temp.append("test_db_table");
  try {
    var con = getService().openDatabase(temp);
    con.createTable("a","");
  } catch (e) {
    if (temp.exists()) try {
      temp.remove(false);
    } catch (e2) {}
    do_check_true(e.result==Cr.NS_ERROR_NOT_INITIALIZED ||
                  e.result==Cr.NS_ERROR_FAILURE);
  } finally {
    if (con) {
      con.close();
    }
  }
});

add_task(function test_defaultSynchronousAtNormal()
{
  var msc = getOpenedDatabase();
  var stmt = createStatement("PRAGMA synchronous;");
  try {
    stmt.executeStep();
    do_check_eq(1, stmt.getInt32(0));
  }
  finally {
    stmt.reset();
    stmt.finalize();
  }
});

// must be ran before executeAsync tests
add_task(function test_close_does_not_spin_event_loop()
{
  // We want to make sure that the event loop on the calling thread does not
  // spin when close is called.
  let event = {
    ran: false,
    run: function()
    {
      this.ran = true;
    },
  };

  // Post the event before we call close, so it would run if the event loop was
  // spun during close.
  let thread = Cc["@mozilla.org/thread-manager;1"].
               getService(Ci.nsIThreadManager).
               currentThread;
  thread.dispatch(event, Ci.nsIThread.DISPATCH_NORMAL);

  // Sanity check, then close the database.  Afterwards, we should not have ran!
  do_check_false(event.ran);
  getOpenedDatabase().close();
  do_check_false(event.ran);

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
});

add_task(function test_asyncClose_succeeds_with_finalized_async_statement()
{
  // XXX this test isn't perfect since we can't totally control when events will
  //     run.  If this paticular function fails randomly, it means we have a
  //     real bug.

  // We want to make sure we create a cached async statement to make sure that
  // when we finalize our statement, we end up finalizing the async one too so
  // close will succeed.
  let stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync();
  stmt.finalize();

  yield asyncClose(getOpenedDatabase());
  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
});

add_task(function test_close_fails_with_async_statement_ran()
{
  let deferred = Promise.defer();
  let stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync();
  stmt.finalize();

  let db = getOpenedDatabase();
  try {
    db.close();
    do_throw("should have thrown");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
  }
  finally {
    // Clean up after ourselves.
    db.asyncClose(function() {
      // Reset gDBConn so that later tests will get a new connection object.
      gDBConn = null;
      deferred.resolve();
    });
  }
  yield deferred.promise;
});

add_task(function test_clone_optional_param()
{
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

function standardAsyncTest(promisedDB, name, shouldInit = false) {
  do_print("Performing standard async test " + name);

  let adb = yield promisedDB;
  do_check_true(adb instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(adb instanceof Ci.mozIStorageConnection);

  if (shouldInit) {
    let stmt = adb.createAsyncStatement("CREATE TABLE test(name TEXT)");
    yield executeAsync(stmt);
    stmt.finalize();
  }

  // Generate a name to insert and fetch back
  name = "worker bee " + Math.random() + " (" + name + ")";

  let stmt = adb.createAsyncStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = name;
  let result = yield executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data");
  stmt = adb.createAsyncStatement("SELECT * FROM test");
  let found = false;
  yield executeAsync(stmt, function(result) {
    do_print("Data has been extracted");

    for (let row = result.getNextRow(); row != null; row = result.getNextRow()) {
      if (row.getResultByName("name") == name) {
        found = true;
      break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  yield asyncClose(adb);

  do_print("Standard async test " + name + " complete");
}

add_task(function test_open_async() {
  yield standardAsyncTest(openAsyncDatabase(getTestDB(), null), "default");
  yield standardAsyncTest(openAsyncDatabase(getTestDB()), "no optional arg");
  yield standardAsyncTest(openAsyncDatabase(getTestDB(),
    {shared: false, growthIncrement: 54}), "non-default options");
  yield standardAsyncTest(openAsyncDatabase("memory"),
    "in-memory database", true);
  yield standardAsyncTest(openAsyncDatabase("memory",
    {shared: false, growthIncrement: 54}),
    "in-memory database and options", true);

  do_print("Testing async opening with bogus options 1");
  let raised = false;
  let adb = null;
  try {
    adb = yield openAsyncDatabase(getTestDB(), {shared: "forty-two"});
  } catch (ex) {
    raised = true;
  } finally {
    if (adb) {
      yield asyncClose(adb);
    }
  }
  do_check_true(raised);

  do_print("Testing async opening with bogus options 2");
  raised = false;
  adb = null;
  try {
    adb = yield openAsyncDatabase(getTestDB(), {growthIncrement: "forty-two"});
  } catch (ex) {
    raised = true;
  } finally {
    if (adb) {
      yield asyncClose(adb);
    }
  }
  do_check_true(raised);
});


add_task(function test_async_open_with_shared_cache() {
  do_print("Testing that opening with a shared cache doesn't break stuff");
  let adb = yield openAsyncDatabase(getTestDB(), {shared: true});

  let stmt = adb.createAsyncStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "clockworker";
  let result = yield executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data");
  stmt = adb.createAsyncStatement("SELECT * FROM test");
  let found = false;
  yield executeAsync(stmt, function(result) {
    do_print("Data has been extracted");

    for (let row = result.getNextRow(); row != null; row = result.getNextRow()) {
      if (row.getResultByName("name") == "clockworker") {
        found = true;
      break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  yield asyncClose(adb);
});

add_task(function test_clone_trivial_async()
{
  let db1 = getService().openDatabase(getTestDB());
  do_print("Opened adb1");
  do_check_true(db1 instanceof Ci.mozIStorageAsyncConnection);
  let adb2 = yield asyncClone(db1, true);
  do_check_true(adb2 instanceof Ci.mozIStorageAsyncConnection);
  do_print("Cloned to adb2");
  db1.close();
  do_print("Closed db1");
  yield asyncClose(adb2);
});

add_task(function test_clone_no_optional_param_async()
{
  "use strict";
  do_print("Testing async cloning");
  let adb1 = yield openAsyncDatabase(getTestDB(), null);
  do_check_true(adb1 instanceof Ci.mozIStorageAsyncConnection);

  do_print("Cloning database");
  do_check_true(Components.isSuccessCode(result));

  let adb2 = yield asyncClone(adb1);
  do_print("Testing that the cloned db is a mozIStorageAsyncConnection " +
           "and not a mozIStorageConnection");
  do_check_true(adb2 instanceof Ci.mozIStorageAsyncConnection);
  do_check_false(adb2 instanceof Ci.mozIStorageConnection);

  do_print("Inserting data into source db");
  let stmt = adb1.
               createAsyncStatement("INSERT INTO test (name) VALUES (:name)");

  stmt.params.name = "yoric";
  let result = yield executeAsync(stmt);
  do_print("Request complete");
  stmt.finalize();
  do_check_true(Components.isSuccessCode(result));
  do_print("Extracting data from clone db");
  stmt = adb2.createAsyncStatement("SELECT * FROM test");
  let found = false;
  yield executeAsync(stmt, function(result) {
    do_print("Data has been extracted");

    for (let row = result.getNextRow(); row != null; row = result.getNextRow()) {
      if (row.getResultByName("name") == "yoric") {
        found = true;
      break;
      }
    }
  });
  do_check_true(found);
  stmt.finalize();
  do_print("Closing databases");
  yield asyncClose(adb2);
  do_print("First db closed");

  yield asyncClose(adb1);
  do_print("Second db closed");
});

add_task(function test_clone_readonly()
{
  let db1 = getService().openUnsharedDatabase(getTestDB());
  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // A write statement should fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "reed";
  expectError(Cr.NS_ERROR_FILE_READ_ONLY, function() stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  db1.close();
  db2.close();
});

add_task(function test_clone_shared_readonly()
{
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
  // expectError(Components.results.NS_ERROR_FILE_READ_ONLY, function() stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  db1.close();
  db2.close();
});

add_task(function test_close_clone_fails()
{
  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  calls.forEach(function(methodName) {
    let db = getService()[methodName](getTestDB());
    db.close();
    expectError(Cr.NS_ERROR_NOT_INITIALIZED, function() db.clone());
  });
});

add_task(function test_memory_clone_fails()
{
  let db = getService().openSpecialDatabase("memory");
  db.close();
  expectError(Cr.NS_ERROR_NOT_INITIALIZED, function() db.clone());
});

add_task(function test_clone_copies_functions()
{
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
          onFunctionCall: function() 0,
          onStep: function() 0,
          onFinal: function() 0,
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

add_task(function test_clone_copies_overridden_functions()
{
  const FUNC_NAME = "lower";
  function test_func() {
    this.called = false;
  }
  test_func.prototype = {
    onFunctionCall: function() {
      this.called = true;
    },
    onStep: function() {
      this.called = true;
    },
    onFinal: function() 0,
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

add_task(function test_clone_copies_pragmas()
{
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: true },
    { name: "journal_size_limit", value: 524288, copied: true },
    { name: "synchronous", value: 2, copied: true },
    { name: "wal_autocheckpoint", value: 16, copied: true },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function (pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  db1.close();
  db2.close();
});

add_task(function test_readonly_clone_copies_pragmas()
{
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: false },
    { name: "journal_size_limit", value: 524288, copied: false },
    { name: "synchronous", value: 2, copied: false },
    { name: "wal_autocheckpoint", value: 16, copied: false },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function (pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  db1.close();
  db2.close();
});

add_task(function test_getInterface()
{
  let db = getOpenedDatabase();
  let target = db.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIEventTarget);
  // Just check that target is non-null.  Other tests will ensure that it has
  // the correct value.
  do_check_true(target != null);

  yield asyncClose(db);
  gDBConn = null;
});


function run_test()
{
  run_next_test();
}
