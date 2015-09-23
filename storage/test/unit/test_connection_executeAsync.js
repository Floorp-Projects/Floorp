/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests the functionality of mozIStorageConnection::executeAsync for
 * both mozIStorageStatement and mozIStorageAsyncStatement.
 */

const INTEGER = 1;
const TEXT = "this is test text";
const REAL = 3.23;
const BLOB = [1, 2];

function test_create_and_add()
{
  getOpenedDatabase().executeSimpleSQL(
    "CREATE TABLE test (" +
      "id INTEGER, " +
      "string TEXT, " +
      "number REAL, " +
      "nuller NULL, " +
      "blober BLOB" +
    ")"
  );

  let stmts = [];
  stmts[0] = getOpenedDatabase().createStatement(
    "INSERT INTO test (id, string, number, nuller, blober) VALUES (?, ?, ?, ?, ?)"
  );
  stmts[0].bindByIndex(0, INTEGER);
  stmts[0].bindByIndex(1, TEXT);
  stmts[0].bindByIndex(2, REAL);
  stmts[0].bindByIndex(3, null);
  stmts[0].bindBlobByIndex(4, BLOB, BLOB.length);
  stmts[1] = getOpenedDatabase().createAsyncStatement(
    "INSERT INTO test (string, number, nuller, blober) VALUES (?, ?, ?, ?)"
  );
  stmts[1].bindByIndex(0, TEXT);
  stmts[1].bindByIndex(1, REAL);
  stmts[1].bindByIndex(2, null);
  stmts[1].bindBlobByIndex(3, BLOB, BLOB.length);

  getOpenedDatabase().executeAsync(stmts, stmts.length, {
    handleResult: function(aResultSet)
    {
      dump("handleResult("+aResultSet+")\n");
      do_throw("unexpected results obtained!");
    },
    handleError: function(aError)
    {
      dump("handleError("+aError.result+")\n");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      dump("handleCompletion("+aReason+")\n");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check that the result is in the table
      let stmt = getOpenedDatabase().createStatement(
        "SELECT string, number, nuller, blober FROM test WHERE id = ?"
      );
      stmt.bindByIndex(0, INTEGER);
      try {
        do_check_true(stmt.executeStep());
        do_check_eq(TEXT, stmt.getString(0));
        do_check_eq(REAL, stmt.getDouble(1));
        do_check_true(stmt.getIsNull(2));
        let count = { value: 0 };
        let blob = { value: null };
        stmt.getBlob(3, count, blob);
        do_check_eq(BLOB.length, count.value);
        for (let i = 0; i < BLOB.length; i++)
          do_check_eq(BLOB[i], blob.value[i]);
      }
      finally {
        stmt.finalize();
      }

      // Make sure we have two rows in the table
      stmt = getOpenedDatabase().createStatement(
        "SELECT COUNT(1) FROM test"
      );
      try {
        do_check_true(stmt.executeStep());
        do_check_eq(2, stmt.getInt32(0));
      }
      finally {
        stmt.finalize();
      }

      // Run the next test.
      run_next_test();
    }
  });
  stmts[0].finalize();
  stmts[1].finalize();
}

function test_multiple_bindings_on_statements()
{
  // This tests to make sure that we pass all the statements multiply bound
  // parameters when we call executeAsync.
  const AMOUNT_TO_ADD = 5;
  const ITERATIONS = 5;

  let stmts = [];
  let db = getOpenedDatabase();
  let sqlString = "INSERT INTO test (id, string, number, nuller, blober) " +
                    "VALUES (:int, :text, :real, :null, :blob)";
  // We run the same statement twice, and should insert 2 * AMOUNT_TO_ADD.
  for (let i = 0; i < ITERATIONS; i++) {
    // alternate the type of statement we create
    if (i % 2)
      stmts[i] = db.createStatement(sqlString);
    else
      stmts[i] = db.createAsyncStatement(sqlString);

    let params = stmts[i].newBindingParamsArray();
    for (let j = 0; j < AMOUNT_TO_ADD; j++) {
      let bp = params.newBindingParams();
      bp.bindByName("int", INTEGER);
      bp.bindByName("text", TEXT);
      bp.bindByName("real", REAL);
      bp.bindByName("null", null);
      bp.bindBlobByName("blob", BLOB, BLOB.length);
      params.addParams(bp);
    }
    stmts[i].bindParameters(params);
  }

  // Get our current number of rows in the table.
  let currentRows = 0;
  let countStmt = getOpenedDatabase().createStatement(
    "SELECT COUNT(1) AS count FROM test"
  );
  try {
    do_check_true(countStmt.executeStep());
    currentRows = countStmt.row.count;
  }
  finally {
    countStmt.reset();
  }

  // Execute asynchronously.
  getOpenedDatabase().executeAsync(stmts, stmts.length, {
    handleResult: function(aResultSet)
    {
      do_throw("Unexpected call to handleResult!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("Unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_multiple_bindings_on_statements");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check to make sure we added all of our rows.
      try {
        do_check_true(countStmt.executeStep());
        do_check_eq(currentRows + (ITERATIONS * AMOUNT_TO_ADD),
                    countStmt.row.count);
      }
      finally {
        countStmt.finalize();
      }

      // Run the next test.
      run_next_test();
    }
  });
  stmts.forEach(stmt => stmt.finalize());
}

function test_asyncClose_does_not_complete_before_statements()
{
  let stmt = createStatement("SELECT * FROM sqlite_master");
  let executed = false;
  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("Unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_asyncClose_does_not_complete_before_statements");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);
      executed = true;
    }
  });
  stmt.finalize();

  getOpenedDatabase().asyncClose(function() {
    // Ensure that the statement executed to completion.
    do_check_true(executed);

    // Reset gDBConn so that later tests will get a new connection object.
    gDBConn = null;
    run_next_test();
  });
}

function test_asyncClose_does_not_throw_no_callback()
{
  getOpenedDatabase().asyncClose();

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
  run_next_test();
}

function test_double_asyncClose_throws()
{
  let conn = getOpenedDatabase();
  conn.asyncClose();
  try {
    conn.asyncClose();
    do_throw("should have thrown");
    // There is a small race condition here, which can cause either of
    // Cr.NS_ERROR_NOT_INITIALIZED or Cr.NS_ERROR_UNEXPECTED to be thrown.
  } catch (e if "result" in e && e.result == Cr.NS_ERROR_NOT_INITIALIZED) {
    do_print("NS_ERROR_NOT_INITIALIZED");
  } catch (e if "result" in e && e.result == Cr.NS_ERROR_UNEXPECTED) {
    do_print("NS_ERROR_UNEXPECTED");
  } catch (e) {
  }

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_create_and_add,
  test_multiple_bindings_on_statements,
  test_asyncClose_does_not_complete_before_statements,
  test_asyncClose_does_not_throw_no_callback,
  test_double_asyncClose_throws,
].forEach(add_test);

function run_test()
{
  cleanup();
  run_next_test();
}
