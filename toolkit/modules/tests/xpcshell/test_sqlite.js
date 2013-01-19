/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

do_get_profile();

Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");


function getConnection(dbName) {
  let path = dbName + ".sqlite";

  return Sqlite.openConnection({path: path});
}

function getDummyDatabase(name) {
  const TABLES = {
    dirs: "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT",
    files: "id INTEGER PRIMARY KEY AUTOINCREMENT, dir_id INTEGER, path TEXT",
  };

  let c = yield getConnection(name);

  for (let [k, v] in Iterator(TABLES)) {
    yield c.execute("CREATE TABLE " + k + "(" + v + ")");
  }

  throw new Task.Result(c);
}


function run_test() {
  Cu.import("resource://testing-common/services-common/logging.js");
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
  do_check_eq(c.lastInsertRowID, 1);
  do_check_eq(c.affectedRows, 1);
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

add_task(function test_close_cached() {
  let c = yield getDummyDatabase("close_cached");

  yield c.executeCached("SELECT * FROM dirs");
  yield c.executeCached("SELECT * FROM files");

  yield c.close();
});

add_task(function test_execute_invalid_statement() {
  let c = yield getDummyDatabase("invalid_statement");

  let deferred = Promise.defer();

  c.execute("SELECT invalid FROM unknown").then(do_throw, function onError(error) {
    deferred.resolve();
  });

  yield deferred.promise;
  yield c.close();
});

add_task(function test_on_row_exception_ignored() {
  let c = yield getDummyDatabase("on_row_exception_ignored");

  let sql = "INSERT INTO dirs (path) VALUES (?)";
  for (let i = 0; i < 10; i++) {
    yield c.executeCached(sql, ["dir" + i]);
  }

  let i = 0;
  yield c.execute("SELECT * FROM DIRS", null, function onRow(row) {
    i++;

    throw new Error("Some silly error.");
  });

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
  let result = yield c.execute("SELECT * FROM dirs", null, function onRow(row) {
    i++;

    if (i == 5) {
      throw StopIteration;
    }
  });

  do_check_null(result);
  do_check_eq(i, 5);

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

