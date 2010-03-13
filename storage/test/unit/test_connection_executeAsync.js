/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Storage Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This file tests the functionality of mozIStorageConnection::executeAsync

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
  stmts[0].bindInt32Parameter(0, INTEGER);
  stmts[0].bindStringParameter(1, TEXT);
  stmts[0].bindDoubleParameter(2, REAL);
  stmts[0].bindNullParameter(3);
  stmts[0].bindBlobParameter(4, BLOB, BLOB.length);
  stmts[1] = getOpenedDatabase().createStatement(
    "INSERT INTO test (string, number, nuller, blober) VALUES (?, ?, ?, ?)"
  );
  stmts[1].bindStringParameter(0, TEXT);
  stmts[1].bindDoubleParameter(1, REAL);
  stmts[1].bindNullParameter(2);
  stmts[1].bindBlobParameter(3, BLOB, BLOB.length);

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
      stmt.bindInt32Parameter(0, INTEGER);
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

function test_transaction_created()
{
  let stmts = [];
  stmts[0] = getOpenedDatabase().createStatement(
    "BEGIN"
  );
  stmts[1] = getOpenedDatabase().createStatement(
    "SELECT * FROM test"
  );

  getOpenedDatabase().executeAsync(stmts, stmts.length, {
    handleResult: function(aResultSet)
    {
      dump("handleResults("+aResultSet+")\n");
      do_throw("unexpected results obtained!");
    },
    handleError: function(aError)
    {
      dump("handleError("+aError.result+")\n");
    },
    handleCompletion: function(aReason)
    {
      dump("handleCompletion("+aReason+")\n");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_ERROR, aReason);

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
  // We run the same statement twice, and should insert 2 * AMOUNT_TO_ADD.
  for (let i = 0; i < ITERATIONS; i++) {
    stmts[i] = getOpenedDatabase().createStatement(
      "INSERT INTO test (id, string, number, nuller, blober) " +
      "VALUES (:int, :text, :real, :null, :blob)"
    );
    let params = stmts[i].newBindingParamsArray()
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
  stmts.forEach(function(stmt) stmt.finalize());
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
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
  }

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests =
[
  test_create_and_add,
  test_transaction_created,
  test_multiple_bindings_on_statements,
  test_asyncClose_does_not_complete_before_statements,
  test_asyncClose_does_not_throw_no_callback,
  test_double_asyncClose_throws,
];
let index = 0;

function run_next_test()
{
  function _run_next_test() {
    if (index < tests.length) {
      do_test_pending();
      print("Running the next test: " + tests[index].name);

      // Asynchronous tests means that exceptions don't kill the test.
      try {
        tests[index++]();
      }
      catch (e) {
        do_throw(e);
      }
    }

    do_test_finished();
  }

  // For saner stacks, we execute this code RSN.
  do_execute_soon(_run_next_test);
}

function run_test()
{
  cleanup();

  do_test_pending();
  run_next_test();
}
