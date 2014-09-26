/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

do_get_profile();

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// To spin the event loop in test.
Cu.import("resource://services-common/async.js");

function sleep(ms) {
  let deferred = Promise.defer();

  let timer = Cc["@mozilla.org/timer;1"]
                .createInstance(Ci.nsITimer);

  timer.initWithCallback({
    notify: function () {
      deferred.resolve();
    },
  }, ms, timer.TYPE_ONE_SHOT);

  return deferred.promise;
}

// When testing finalization, use this to tell Sqlite.jsm to not throw
// an uncatchable `Promise.reject`
function failTestsOnAutoClose(enabled)  {
  Cu.getGlobalForObject(Sqlite).Debugging.failTestsOnAutoClose = enabled;
}

function getConnection(dbName, extraOptions={}) {
  let path = dbName + ".sqlite";
  let options = {path: path};
  for (let [k, v] in Iterator(extraOptions)) {
    options[k] = v;
  }

  return Sqlite.openConnection(options);
}

function getDummyDatabase(name, extraOptions={}) {
  const TABLES = {
    dirs: "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT",
    files: "id INTEGER PRIMARY KEY AUTOINCREMENT, dir_id INTEGER, path TEXT",
  };

  let c = yield getConnection(name, extraOptions);
  c._initialStatementCount = 0;

  for (let [k, v] in Iterator(TABLES)) {
    yield c.execute("CREATE TABLE " + k + "(" + v + ")");
    c._initialStatementCount++;
  }

  throw new Task.Result(c);
}

function getDummyTempDatabase(name, extraOptions={}) {
  const TABLES = {
    dirs: "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT",
    files: "id INTEGER PRIMARY KEY AUTOINCREMENT, dir_id INTEGER, path TEXT",
  };

  let c = yield getConnection(name, extraOptions);
  c._initialStatementCount = 0;

  for (let [k, v] in Iterator(TABLES)) {
    yield c.execute("CREATE TEMP TABLE " + k + "(" + v + ")");
    c._initialStatementCount++;
  }

  throw new Task.Result(c);
}

function run_test() {
  Cu.import("resource://testing-common/services/common/logging.js");
  initTestLogging("Trace");

  run_next_test();
}

add_task(function test_open_normal() {
  let c = yield Sqlite.openConnection({path: "test_open_normal.sqlite"});
  yield c.close();
});

add_task(function test_open_unshared() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "test_open_unshared.sqlite");

  let c = yield Sqlite.openConnection({path: path, sharedMemoryCache: false});
  yield c.close();
});

add_task(function test_get_dummy_database() {
  let db = yield getDummyDatabase("get_dummy_database");

  do_check_eq(typeof(db), "object");
  yield db.close();
});

add_task(function test_schema_version() {
  let db = yield getDummyDatabase("schema_version");

  let version = yield db.getSchemaVersion();
  do_check_eq(version, 0);

  db.setSchemaVersion(14);
  version = yield db.getSchemaVersion();
  do_check_eq(version, 14);

  for (let v of [0.5, "foobar", NaN]) {
    let success;
    try {
      yield db.setSchemaVersion(v);
      do_print("Schema version " + v + " should have been rejected");
      success = false;
    } catch (ex if ex.message.startsWith("Schema version must be an integer.")) {
      success = true;
    }
    do_check_true(success);

    version = yield db.getSchemaVersion();
    do_check_eq(version, 14);
  }

  yield db.close();
});

add_task(function test_simple_insert() {
  let c = yield getDummyDatabase("simple_insert");

  let result = yield c.execute("INSERT INTO dirs VALUES (NULL, 'foo')");
  do_check_true(Array.isArray(result));
  do_check_eq(result.length, 0);
  yield c.close();
});

add_task(function test_simple_bound_array() {
  let c = yield getDummyDatabase("simple_bound_array");

  let result = yield c.execute("INSERT INTO dirs VALUES (?, ?)", [1, "foo"]);
  do_check_eq(result.length, 0);
  yield c.close();
});

add_task(function test_simple_bound_object() {
  let c = yield getDummyDatabase("simple_bound_object");
  let result = yield c.execute("INSERT INTO dirs VALUES (:id, :path)",
                               {id: 1, path: "foo"});
  do_check_eq(result.length, 0);
  result = yield c.execute("SELECT id, path FROM dirs");
  do_check_eq(result.length, 1);
  do_check_eq(result[0].getResultByName("id"), 1);
  do_check_eq(result[0].getResultByName("path"), "foo");
  yield c.close();
});

// This is mostly a sanity test to ensure simple executions work.
add_task(function test_simple_insert_then_select() {
  let c = yield getDummyDatabase("simple_insert_then_select");

  yield c.execute("INSERT INTO dirs VALUES (NULL, 'foo')");
  yield c.execute("INSERT INTO dirs (path) VALUES (?)", ["bar"]);

  let result = yield c.execute("SELECT * FROM dirs");
  do_check_eq(result.length, 2);

  let i = 0;
  for (let row of result) {
    i++;

    do_check_eq(row.numEntries, 2);
    do_check_eq(row.getResultByIndex(0), i);

    let expected = {1: "foo", 2: "bar"}[i];
    do_check_eq(row.getResultByName("path"), expected);
  }

  yield c.close();
});

add_task(function test_repeat_execution() {
  let c = yield getDummyDatabase("repeat_execution");

  let sql = "INSERT INTO dirs (path) VALUES (:path)";
  yield c.executeCached(sql, {path: "foo"});
  yield c.executeCached(sql);

  let result = yield c.execute("SELECT * FROM dirs");

  do_check_eq(result.length, 2);

  yield c.close();
});

add_task(function test_table_exists() {
  let c = yield getDummyDatabase("table_exists");

  do_check_false(yield c.tableExists("does_not_exist"));
  do_check_true(yield c.tableExists("dirs"));
  do_check_true(yield c.tableExists("files"));

  yield c.close();
});

add_task(function test_index_exists() {
  let c = yield getDummyDatabase("index_exists");

  do_check_false(yield c.indexExists("does_not_exist"));

  yield c.execute("CREATE INDEX my_index ON dirs (path)");
  do_check_true(yield c.indexExists("my_index"));

  yield c.close();
});

add_task(function test_temp_table_exists() {
  let c = yield getDummyTempDatabase("temp_table_exists");

  do_check_false(yield c.tableExists("temp_does_not_exist"));
  do_check_true(yield c.tableExists("dirs"));
  do_check_true(yield c.tableExists("files"));

  yield c.close();
});

add_task(function test_temp_index_exists() {
  let c = yield getDummyTempDatabase("temp_index_exists");

  do_check_false(yield c.indexExists("temp_does_not_exist"));

  yield c.execute("CREATE INDEX my_index ON dirs (path)");
  do_check_true(yield c.indexExists("my_index"));

  yield c.close();
});

add_task(function test_close_cached() {
  let c = yield getDummyDatabase("close_cached");

  yield c.executeCached("SELECT * FROM dirs");
  yield c.executeCached("SELECT * FROM files");

  yield c.close();
});

add_task(function test_execute_invalid_statement() {
  let c = yield getDummyDatabase("invalid_statement");

  let deferred = Promise.defer();

  do_check_eq(c._connectionData._anonymousStatements.size, 0);

  c.execute("SELECT invalid FROM unknown").then(do_throw, function onError(error) {
    deferred.resolve();
  });

  yield deferred.promise;

  // Ensure we don't leak the statement instance.
  do_check_eq(c._connectionData._anonymousStatements.size, 0);

  yield c.close();
});

add_task(function test_on_row_exception_ignored() {
  let c = yield getDummyDatabase("on_row_exception_ignored");

  let sql = "INSERT INTO dirs (path) VALUES (?)";
  for (let i = 0; i < 10; i++) {
    yield c.executeCached(sql, ["dir" + i]);
  }

  let i = 0;
  let hasResult = yield c.execute("SELECT * FROM DIRS", null, function onRow(row) {
    i++;

    throw new Error("Some silly error.");
  });

  do_check_eq(hasResult, true);
  do_check_eq(i, 10);

  yield c.close();
});

// Ensure StopIteration during onRow causes processing to stop.
add_task(function test_on_row_stop_iteration() {
  let c = yield getDummyDatabase("on_row_stop_iteration");

  let sql = "INSERT INTO dirs (path) VALUES (?)";
  for (let i = 0; i < 10; i++) {
    yield c.executeCached(sql, ["dir" + i]);
  }

  let i = 0;
  let hasResult = yield c.execute("SELECT * FROM dirs", null, function onRow(row) {
    i++;

    if (i == 5) {
      throw StopIteration;
    }
  });

  do_check_eq(hasResult, true);
  do_check_eq(i, 5);

  yield c.close();
});

// Ensure execute resolves to false when no rows are selected.
add_task(function test_on_row_stop_iteration() {
  let c = yield getDummyDatabase("no_on_row");

  let i = 0;
  let hasResult = yield c.execute(`SELECT * FROM dirs WHERE path="nonexistent"`, null, function onRow(row) {
    i++;
  });

  do_check_eq(hasResult, false);
  do_check_eq(i, 0);

  yield c.close();
});

add_task(function test_invalid_transaction_type() {
  let c = yield getDummyDatabase("invalid_transaction_type");

  let errored = false;
  try {
    c.executeTransaction(function () {}, "foobar");
  } catch (ex) {
    errored = true;
    do_check_true(ex.message.startsWith("Unknown transaction type"));
  } finally {
    do_check_true(errored);
  }

  yield c.close();
});

add_task(function test_execute_transaction_success() {
  let c = yield getDummyDatabase("execute_transaction_success");

  do_check_false(c.transactionInProgress);

  yield c.executeTransaction(function transaction(conn) {
    do_check_eq(c, conn);
    do_check_true(conn.transactionInProgress);

    yield conn.execute("INSERT INTO dirs (path) VALUES ('foo')");
  });

  do_check_false(c.transactionInProgress);
  let rows = yield c.execute("SELECT * FROM dirs");
  do_check_true(Array.isArray(rows));
  do_check_eq(rows.length, 1);

  yield c.close();
});

add_task(function test_execute_transaction_rollback() {
  let c = yield getDummyDatabase("execute_transaction_rollback");

  let deferred = Promise.defer();

  c.executeTransaction(function transaction(conn) {
    yield conn.execute("INSERT INTO dirs (path) VALUES ('foo')");
    print("Expecting error with next statement.");
    yield conn.execute("INSERT INTO invalid VALUES ('foo')");

    // We should never get here.
    do_throw();
  }).then(do_throw, function onError(error) {
    deferred.resolve();
  });

  yield deferred.promise;

  let rows = yield c.execute("SELECT * FROM dirs");
  do_check_eq(rows.length, 0);

  yield c.close();
});

add_task(function test_close_during_transaction() {
  let c = yield getDummyDatabase("close_during_transaction");

  yield c.execute("INSERT INTO dirs (path) VALUES ('foo')");

  let errored = false;
  try {
    yield c.executeTransaction(function transaction(conn) {
      yield c.execute("INSERT INTO dirs (path) VALUES ('bar')");
      yield c.close();
    });
  } catch (ex) {
    errored = true;
    do_check_eq(ex.message, "Connection being closed.");
  } finally {
    do_check_true(errored);
  }

  let c2 = yield getConnection("close_during_transaction");
  let rows = yield c2.execute("SELECT * FROM dirs");
  do_check_eq(rows.length, 1);

  yield c2.close();
});

add_task(function test_detect_multiple_transactions() {
  let c = yield getDummyDatabase("detect_multiple_transactions");

  yield c.executeTransaction(function main() {
    yield c.execute("INSERT INTO dirs (path) VALUES ('foo')");

    let errored = false;
    try {
      yield c.executeTransaction(function child() {
        yield c.execute("INSERT INTO dirs (path) VALUES ('bar')");
      });
    } catch (ex) {
      errored = true;
      do_check_true(ex.message.startsWith("A transaction is already active."));
    } finally {
      do_check_true(errored);
    }
  });

  let rows = yield c.execute("SELECT * FROM dirs");
  do_check_eq(rows.length, 1);

  yield c.close();
});

add_task(function test_shrink_memory() {
  let c = yield getDummyDatabase("shrink_memory");

  // It's just a simple sanity test. We have no way of measuring whether this
  // actually does anything.

  yield c.shrinkMemory();
  yield c.close();
});

add_task(function test_no_shrink_on_init() {
  let c = yield getConnection("no_shrink_on_init",
                              {shrinkMemoryOnConnectionIdleMS: 200});

  let oldShrink = c._connectionData.shrinkMemory;
  let count = 0;
  Object.defineProperty(c._connectionData, "shrinkMemory", {
    value: function () {
      count++;
    },
  });

  // We should not shrink until a statement has been executed.
  yield sleep(220);
  do_check_eq(count, 0);

  yield c.execute("SELECT 1");
  yield sleep(220);
  do_check_eq(count, 1);

  yield c.close();
});

add_task(function test_idle_shrink_fires() {
  let c = yield getDummyDatabase("idle_shrink_fires",
                                 {shrinkMemoryOnConnectionIdleMS: 200});
  c._connectionData._clearIdleShrinkTimer();

  let oldShrink = c._connectionData.shrinkMemory;
  let shrinkPromises = [];

  let count = 0;
  Object.defineProperty(c._connectionData, "shrinkMemory", {
    value: function () {
      count++;
      let promise = oldShrink.call(c._connectionData);
      shrinkPromises.push(promise);
      return promise;
    },
  });

  // We reset the idle shrink timer after monkeypatching because otherwise the
  // installed timer callback will reference the non-monkeypatched function.
  c._connectionData._startIdleShrinkTimer();

  yield sleep(220);
  do_check_eq(count, 1);
  do_check_eq(shrinkPromises.length, 1);
  yield shrinkPromises[0];
  shrinkPromises.shift();

  // We shouldn't shrink again unless a statement was executed.
  yield sleep(300);
  do_check_eq(count, 1);

  yield c.execute("SELECT 1");
  yield sleep(300);

  do_check_eq(count, 2);
  do_check_eq(shrinkPromises.length, 1);
  yield shrinkPromises[0];

  yield c.close();
});

add_task(function test_idle_shrink_reset_on_operation() {
  const INTERVAL = 500;
  let c = yield getDummyDatabase("idle_shrink_reset_on_operation",
                                 {shrinkMemoryOnConnectionIdleMS: INTERVAL});

  c._connectionData._clearIdleShrinkTimer();

  let oldShrink = c._connectionData.shrinkMemory;
  let shrinkPromises = [];
  let count = 0;

  Object.defineProperty(c._connectionData, "shrinkMemory", {
    value: function () {
      count++;
      let promise = oldShrink.call(c._connectionData);
      shrinkPromises.push(promise);
      return promise;
    },
  });

  let now = new Date();
  c._connectionData._startIdleShrinkTimer();

  let initialIdle = new Date(now.getTime() + INTERVAL);

  // Perform database operations until initial scheduled time has been passed.
  let i = 0;
  while (new Date() < initialIdle) {
    yield c.execute("INSERT INTO dirs (path) VALUES (?)", ["" + i]);
    i++;
  }

  do_check_true(i > 0);

  // We should not have performed an idle while doing operations.
  do_check_eq(count, 0);

  // Wait for idle timer.
  yield sleep(INTERVAL);

  // Ensure we fired.
  do_check_eq(count, 1);
  do_check_eq(shrinkPromises.length, 1);
  yield shrinkPromises[0];

  yield c.close();
});

add_task(function test_in_progress_counts() {
  let c = yield getDummyDatabase("in_progress_counts");
  do_check_eq(c._connectionData._statementCounter, c._initialStatementCount);
  do_check_eq(c._connectionData._pendingStatements.size, 0);
  yield c.executeCached("INSERT INTO dirs (path) VALUES ('foo')");
  do_check_eq(c._connectionData._statementCounter, c._initialStatementCount + 1);
  do_check_eq(c._connectionData._pendingStatements.size, 0);

  let expectOne;
  let expectTwo;

  // Please forgive me.
  let inner = Async.makeSpinningCallback();
  let outer = Async.makeSpinningCallback();

  // We want to make sure that two queries executing simultaneously
  // result in `_pendingStatements.size` reaching 2, then dropping back to 0.
  //
  // To do so, we kick off a second statement within the row handler
  // of the first, then wait for both to finish.

  yield c.executeCached("SELECT * from dirs", null, function onRow() {
    // In the onRow handler, we're still an outstanding query.
    // Expect a single in-progress entry.
    expectOne = c._connectionData._pendingStatements.size;

    // Start another query, checking that after its statement has been created
    // there are two statements in progress.
    let p = c.executeCached("SELECT 10, path from dirs");
    expectTwo = c._connectionData._pendingStatements.size;

    // Now wait for it to be done before we return from the row handler …
    p.then(function onInner() {
      inner();
    });
  }).then(function onOuter() {
    // … and wait for the inner to be done before we finish …
    inner.wait();
    outer();
  });

  // … and wait for both queries to have finished before we go on and
  // test postconditions.
  outer.wait();

  do_check_eq(expectOne, 1);
  do_check_eq(expectTwo, 2);
  do_check_eq(c._connectionData._statementCounter, c._initialStatementCount + 3);
  do_check_eq(c._connectionData._pendingStatements.size, 0);

  yield c.close();
});

add_task(function test_discard_while_active() {
  let c = yield getDummyDatabase("discard_while_active");

  yield c.executeCached("INSERT INTO dirs (path) VALUES ('foo')");
  yield c.executeCached("INSERT INTO dirs (path) VALUES ('bar')");

  let discarded = -1;
  let first = true;
  let sql = "SELECT * FROM dirs";
  yield c.executeCached(sql, null, function onRow(row) {
    if (!first) {
      return;
    }
    first = false;
    discarded = c.discardCachedStatements();
  });

  // We discarded everything, because the SELECT had already started to run.
  do_check_eq(3, discarded);

  // And again is safe.
  do_check_eq(0, c.discardCachedStatements());

  yield c.close();
});

add_task(function test_discard_cached() {
  let c = yield getDummyDatabase("discard_cached");

  yield c.executeCached("SELECT * from dirs");
  do_check_eq(1, c._connectionData._cachedStatements.size);

  yield c.executeCached("SELECT * from files");
  do_check_eq(2, c._connectionData._cachedStatements.size);

  yield c.executeCached("SELECT * from dirs");
  do_check_eq(2, c._connectionData._cachedStatements.size);

  c.discardCachedStatements();
  do_check_eq(0, c._connectionData._cachedStatements.size);

  yield c.close();
});

add_task(function test_programmatic_binding() {
  let c = yield getDummyDatabase("programmatic_binding");

  let bindings = [
    {id: 1,    path: "foobar"},
    {id: null, path: "baznoo"},
    {id: 5,    path: "toofoo"},
  ];

  let sql = "INSERT INTO dirs VALUES (:id, :path)";
  let result = yield c.execute(sql, bindings);
  do_check_eq(result.length, 0);

  let rows = yield c.executeCached("SELECT * from dirs");
  do_check_eq(rows.length, 3);
  yield c.close();
});

add_task(function test_programmatic_binding_transaction() {
  let c = yield getDummyDatabase("programmatic_binding_transaction");

  let bindings = [
    {id: 1,    path: "foobar"},
    {id: null, path: "baznoo"},
    {id: 5,    path: "toofoo"},
  ];

  let sql = "INSERT INTO dirs VALUES (:id, :path)";
  yield c.executeTransaction(function transaction() {
    let result = yield c.execute(sql, bindings);
    do_check_eq(result.length, 0);

    let rows = yield c.executeCached("SELECT * from dirs");
    do_check_eq(rows.length, 3);
  });

  // Transaction committed.
  let rows = yield c.executeCached("SELECT * from dirs");
  do_check_eq(rows.length, 3);
  yield c.close();
});

add_task(function test_programmatic_binding_transaction_partial_rollback() {
  let c = yield getDummyDatabase("programmatic_binding_transaction_partial_rollback");

  let bindings = [
    {id: 2, path: "foobar"},
    {id: 3, path: "toofoo"},
  ];

  let sql = "INSERT INTO dirs VALUES (:id, :path)";

  // Add some data in an implicit transaction before beginning the batch insert.
  yield c.execute(sql, {id: 1, path: "works"});

  let secondSucceeded = false;
  try {
    yield c.executeTransaction(function transaction() {
      // Insert one row. This won't implicitly start a transaction.
      let result = yield c.execute(sql, bindings[0]);

      // Insert multiple rows. mozStorage will want to start a transaction.
      // One of the inserts will fail, so the transaction should be rolled back.
      result = yield c.execute(sql, bindings);
      secondSucceeded = true;
    });
  } catch (ex) {
    print("Caught expected exception: " + ex);
  }

  // We did not get to the end of our in-transaction block.
  do_check_false(secondSucceeded);

  // Everything that happened in *our* transaction, not mozStorage's, got
  // rolled back, but the first row still exists.
  let rows = yield c.executeCached("SELECT * from dirs");
  do_check_eq(rows.length, 1);
  do_check_eq(rows[0].getResultByName("path"), "works");
  yield c.close();
});

/**
 * Just like the previous test, but relying on the implicit
 * transaction established by mozStorage.
 */
add_task(function test_programmatic_binding_implicit_transaction() {
  let c = yield getDummyDatabase("programmatic_binding_implicit_transaction");

  let bindings = [
    {id: 2, path: "foobar"},
    {id: 1, path: "toofoo"},
  ];

  let sql = "INSERT INTO dirs VALUES (:id, :path)";
  let secondSucceeded = false;
  yield c.execute(sql, {id: 1, path: "works"});
  try {
    let result = yield c.execute(sql, bindings);
    secondSucceeded = true;
  } catch (ex) {
    print("Caught expected exception: " + ex);
  }

  do_check_false(secondSucceeded);

  // The entire batch failed.
  let rows = yield c.executeCached("SELECT * from dirs");
  do_check_eq(rows.length, 1);
  do_check_eq(rows[0].getResultByName("path"), "works");
  yield c.close();
});

/**
 * Test that direct binding of params and execution through mozStorage doesn't
 * error when we manually create a transaction. See Bug 856925.
 */
add_task(function test_direct() {
  let file = FileUtils.getFile("TmpD", ["test_direct.sqlite"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  print("Opening " + file.path);

  let db = Services.storage.openDatabase(file);
  print("Opened " + db);

  db.executeSimpleSQL("CREATE TABLE types (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, UNIQUE (name))");
  print("Executed setup.");

  let statement = db.createAsyncStatement("INSERT INTO types (name) VALUES (:name)");
  let params = statement.newBindingParamsArray();
  let one = params.newBindingParams();
  one.bindByName("name", null);
  params.addParams(one);
  let two = params.newBindingParams();
  two.bindByName("name", "bar");
  params.addParams(two);

  print("Beginning transaction.");
  let begin = db.createAsyncStatement("BEGIN DEFERRED TRANSACTION");
  let end = db.createAsyncStatement("COMMIT TRANSACTION");

  let deferred = Promise.defer();
  begin.executeAsync({
    handleCompletion: function (reason) {
      deferred.resolve();
    }
  });
  yield deferred.promise;

  statement.bindParameters(params);

  deferred = Promise.defer();
  print("Executing async.");
  statement.executeAsync({
    handleResult: function (resultSet) {
    },

    handleError:  function (error) {
      print("Error when executing SQL (" + error.result + "): " +
            error.message);
      print("Original error: " + error.error);
      errors.push(error);
      deferred.reject();
    },

    handleCompletion: function (reason) {
      print("Completed.");
      deferred.resolve();
    }
  });

  yield deferred.promise;

  deferred = Promise.defer();
  end.executeAsync({
    handleCompletion: function (reason) {
      deferred.resolve();
    }
  });
  yield deferred.promise;

  statement.finalize();
  begin.finalize();
  end.finalize();

  deferred = Promise.defer();
  db.asyncClose(function () {
    deferred.resolve()
  });
  yield deferred.promise;
});

/**
 * Test Sqlite.cloneStorageConnection.
 */
add_task(function* test_cloneStorageConnection() {
  let file = new FileUtils.File(OS.Path.join(OS.Constants.Path.profileDir,
                                             "test_cloneStorageConnection.sqlite"));
  let c = yield new Promise((resolve, reject) => {
    Services.storage.openAsyncDatabase(file, null, (status, db) => {
      if (Components.isSuccessCode(status)) {
        resolve(db.QueryInterface(Ci.mozIStorageAsyncConnection));
      } else {
        reject(new Error(status));
      }
    });
  });

  let clone = yield Sqlite.cloneStorageConnection({ connection: c, readOnly: true });
  // Just check that it works.
  yield clone.execute("SELECT 1");

  let clone2 = yield Sqlite.cloneStorageConnection({ connection: c, readOnly: false });
  // Just check that it works.
  yield clone2.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)");

  // Closing order should not matter.
  yield c.asyncClose();
  yield clone2.close();
  yield clone.close();
});

/**
 * Test Sqlite.cloneStorageConnection invalid argument.
 */
add_task(function* test_cloneStorageConnection() {
  try {
    let clone = yield Sqlite.cloneStorageConnection({ connection: null });
    do_throw(new Error("Should throw on invalid connection"));
  } catch (ex if ex.name == "TypeError") {}
});

/**
 * Test clone() method.
 */
add_task(function* test_clone() {
  let c = yield getDummyDatabase("clone");

  let clone = yield c.clone();
  // Just check that it works.
  yield clone.execute("SELECT 1");
  // Closing order should not matter.
  yield c.close();
  yield clone.close();
});

/**
 * Test clone(readOnly) method.
 */
add_task(function* test_readOnly_clone() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, "test_readOnly_clone.sqlite");
  let c = yield Sqlite.openConnection({path: path, sharedMemoryCache: false});

  let clone = yield c.clone(true);
  // Just check that it works.
  yield clone.execute("SELECT 1");
  // But should not be able to write.
  try {
    yield clone.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)");
    do_throw(new Error("Should not be able to write to a read-only clone."));
  } catch (ex) {}
  // Closing order should not matter.
  yield c.close();
  yield clone.close();
});

/**
 * Test Sqlite.wrapStorageConnection.
 */
add_task(function* test_wrapStorageConnection() {
  let file = new FileUtils.File(OS.Path.join(OS.Constants.Path.profileDir,
                                             "test_wrapStorageConnection.sqlite"));
  let c = yield new Promise((resolve, reject) => {
    Services.storage.openAsyncDatabase(file, null, (status, db) => {
      if (Components.isSuccessCode(status)) {
        resolve(db.QueryInterface(Ci.mozIStorageAsyncConnection));
      } else {
        reject(new Error(status));
      }
    });
  });

  let wrapper = yield Sqlite.wrapStorageConnection({ connection: c });
  // Just check that it works.
  yield wrapper.execute("SELECT 1");
  yield wrapper.executeCached("SELECT 1");

  // Closing the wrapper should just finalize statements but not close the
  // database.
  yield wrapper.close();
  yield c.asyncClose();
});

/**
 * Test finalization
 */
add_task(function* test_closed_by_witness() {
  failTestsOnAutoClose(false);
  let c = yield getDummyDatabase("closed_by_witness");

  Services.obs.notifyObservers(null, "sqlite-finalization-witness",
                               c._connectionData._identifier);
  // Since we triggered finalization ourselves, tell the witness to
  // forget the connection so it does not trigger a finalization again
  c._witness.forget();
  yield c._connectionData._deferredClose.promise;
  do_check_false(c._connectionData._open);
  failTestsOnAutoClose(true);
});

add_task(function* test_warning_message_on_finalization() {
  failTestsOnAutoClose(false);
  let c = yield getDummyDatabase("warning_message_on_finalization");
  let identifier = c._connectionData._identifier;
  let deferred = Promise.defer();

  let listener = {
    observe: function(msg) {
      let messageText = msg.message;
      // Make sure the message starts with a warning containing the
      // connection identifier
      if (messageText.indexOf("Warning: Sqlite connection '" + identifier + "'") !== -1) {
        deferred.resolve();
      }
    }
  };
  Services.console.registerListener(listener);

  Services.obs.notifyObservers(null, "sqlite-finalization-witness", identifier);
  // Since we triggered finalization ourselves, tell the witness to
  // forget the connection so it does not trigger a finalization again
  c._witness.forget();

  yield deferred.promise;
  Services.console.unregisterListener(listener);
  failTestsOnAutoClose(true);
});

add_task(function* test_error_message_on_unknown_finalization() {
  failTestsOnAutoClose(false);
  let deferred = Promise.defer();

  let listener = {
    observe: function(msg) {
      let messageText = msg.message;
      if (messageText.indexOf("Error: Attempt to finalize unknown " +
                              "Sqlite connection: foo") !== -1) {
        deferred.resolve();
      }
    }
  };
  Services.console.registerListener(listener);
  Services.obs.notifyObservers(null, "sqlite-finalization-witness", "foo");

  yield deferred.promise;
  Services.console.unregisterListener(listener);
  failTestsOnAutoClose(true);
});

add_task(function* test_forget_witness_on_close() {
  let c = yield getDummyDatabase("forget_witness_on_close");

  let forgetCalled = false;
  let oldWitness = c._witness;
  c._witness = {
    forget: function () {
      forgetCalled = true;
      oldWitness.forget();
    },
  };

  yield c.close();
  // After close, witness should have forgotten the connection
  do_check_true(forgetCalled);
});

add_task(function* test_close_database_on_gc() {
  failTestsOnAutoClose(false);
  let deferred = Promise.defer();

  for (let i = 0; i < 100; ++i) {
    let c = yield getDummyDatabase("gc_" + i);
    c._connectionData._deferredClose.promise.then(deferred.resolve);
  }

  // Call getDummyDatabase once more to clear any remaining
  // references. This is needed at the moment, otherwise
  // garbage-collection takes place after the shutdown barrier and the
  // test will timeout. Once that is fixed, we can remove this line
  // and be fine as long as at least one connection is
  // garbage-collected.
  let last = yield getDummyDatabase("gc_last");
  yield last.close();

  Components.utils.forceGC();
  yield deferred.promise;
  failTestsOnAutoClose(true);
});
