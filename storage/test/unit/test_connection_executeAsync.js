/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests the functionality of mozIStorageConnection::executeAsync for
 * both mozIStorageStatement and mozIStorageAsyncStatement.
 *
 * A single database connection is used for the entirety of the test, which is
 * a legacy thing, but we otherwise use the modern promise-based driver and
 * async helpers.
 */

const INTEGER = 1;
const TEXT = "this is test text";
const REAL = 3.23;
const BLOB = [1, 2];

add_task(function* test_first_create_and_add() {
  // synchronously open the database and let gDBConn hold onto it because we
  // use this database
  let db = getOpenedDatabase();
  // synchronously set up our table *that will be used for the rest of the file*
  db.executeSimpleSQL(
    "CREATE TABLE test (" +
      "id INTEGER, " +
      "string TEXT, " +
      "number REAL, " +
      "nuller NULL, " +
      "blober BLOB" +
    ")"
  );

  let stmts = [];
  stmts[0] = db.createStatement(
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

  // asynchronously execute the statements
  let execResult = yield executeMultipleStatementsAsync(
    db,
    stmts,
    function(aResultSet) {
      ok(false, 'we only did inserts so we should not have gotten results!');
    });
  equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, execResult,
        'execution should have finished successfully.');

  // Check that the result is in the table
  let stmt = db.createStatement(
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
  stmt = db.createStatement(
    "SELECT COUNT(1) FROM test"
  );
  try {
    do_check_true(stmt.executeStep());
    do_check_eq(2, stmt.getInt32(0));
  }
  finally {
    stmt.finalize();
  }

  stmts[0].finalize();
  stmts[1].finalize();
});

add_task(function* test_last_multiple_bindings_on_statements() {
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
  let execResult = yield executeMultipleStatementsAsync(
    db,
    stmts,
    function(aResultSet) {
      ok(false, 'we only did inserts so we should not have gotten results!');
    });
  equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, execResult,
        'execution should have finished successfully.');

  // Check to make sure we added all of our rows.
  try {
    do_check_true(countStmt.executeStep());
    do_check_eq(currentRows + (ITERATIONS * AMOUNT_TO_ADD),
                countStmt.row.count);
  }
  finally {
    countStmt.finalize();
  }

  stmts.forEach(stmt => stmt.finalize());

  // we are the last test using this connection and since it has gone async
  // we *must* call asyncClose on it.
  yield asyncClose(db);
  gDBConn = null;
});

// If you add a test down here you will need to move the asyncClose or clean
// things up a little more.
