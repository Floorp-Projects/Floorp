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

// This file tests the functionality of mozIStorageStatement::executeAsync

const INTEGER = 1;
const TEXT = "this is test text";
const REAL = 3.23;
const BLOB = [1, 2];

function test_create_table()
{
  // Ensure our table doesn't exists
  do_check_false(getOpenedDatabase().tableExists("test"));

  var stmt = getOpenedDatabase().createStatement(
    "CREATE TABLE test (" +
      "id INTEGER, " +
      "string TEXT, " +
      "number REAL, " +
      "nuller NULL, " +
      "blober BLOB" +
    ")"
  );

  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      dump("handleResult("+aResultSet+");\n");
      do_throw("unexpected results obtained!");
    },
    handleError: function(aError)
    {
      print("error code " + aerror.result + " with message '" +
            aerror.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason + ") for test_create_table");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check that the table has been created
      do_check_true(getOpenedDatabase().tableExists("test"));

      // Verify that it's created correctly (this will throw if it wasn't)
      var stmt = getOpenedDatabase().createStatement(
        "SELECT id, string, number, nuller, blober FROM test"
      );
      stmt.finalize();

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_add_data()
{
  var stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (?, ?, ?, ?, ?)"
  );
  stmt.bindBlobParameter(4, BLOB, BLOB.length);
  stmt.bindNullParameter(3);
  stmt.bindDoubleParameter(2, REAL);
  stmt.bindStringParameter(1, TEXT);
  stmt.bindInt32Parameter(0, INTEGER);

  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      do_throw("unexpected results obtained!");
    },
    handleError: function(aError)
    {
      print("error code " + aerror.result + " with message '" +
            aerror.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason + ") for test_add_data");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check that the result is in the table
      var stmt = getOpenedDatabase().createStatement(
        "SELECT string, number, nuller, blober FROM test WHERE id = ?"
      );
      stmt.bindInt32Parameter(0, INTEGER);
      try {
        do_check_true(stmt.executeStep());
        do_check_eq(TEXT, stmt.getString(0));
        do_check_eq(REAL, stmt.getDouble(1));
        do_check_true(stmt.getIsNull(2));
        var count = { value: 0 };
        var blob = { value: null };
        stmt.getBlob(3, count, blob);
        do_check_eq(BLOB.length, count.value);
        for (var i = 0; i < BLOB.length; i++)
          do_check_eq(BLOB[i], blob.value[i]);
      }
      finally {
        stmt.reset();
        stmt.finalize();
      }

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_get_data()
{
  var stmt = getOpenedDatabase().createStatement(
    "SELECT string, number, nuller, blober, id FROM test WHERE id = ?"
  );
  stmt.bindInt32Parameter(0, INTEGER);

  stmt.executeAsync({
    resultObtained: false,
    handleResult: function(aResultSet)
    {
      dump("handleResult("+aResultSet+");\n");
      do_check_false(this.resultObtained);
      this.resultObtained = true;

      // Check that we have a result
      var tuple = aResultSet.getNextRow();
      do_check_neq(null, tuple);

      // Check that it's what we expect
      do_check_false(tuple.getIsNull(0));
      do_check_eq(tuple.getResultByName("string"), tuple.getResultByIndex(0));
      do_check_eq(TEXT, tuple.getResultByName("string"));
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_TEXT,
                  tuple.getTypeOfIndex(0));

      do_check_false(tuple.getIsNull(1));
      do_check_eq(tuple.getResultByName("number"), tuple.getResultByIndex(1));
      do_check_eq(REAL, tuple.getResultByName("number"));
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_FLOAT,
                  tuple.getTypeOfIndex(1));

      do_check_true(tuple.getIsNull(2));
      do_check_eq(tuple.getResultByName("nuller"), tuple.getResultByIndex(2));
      do_check_eq(null, tuple.getResultByName("nuller"));
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_NULL,
                  tuple.getTypeOfIndex(2));

      do_check_false(tuple.getIsNull(3));
      var blobByName = tuple.getResultByName("blober");
      do_check_eq(BLOB.length, blobByName.length);
      var blobByIndex = tuple.getResultByIndex(3);
      do_check_eq(BLOB.length, blobByIndex.length);
      for (var i = 0; i < BLOB.length; i++) {
        do_check_eq(BLOB[i], blobByName[i]);
        do_check_eq(BLOB[i], blobByIndex[i]);
      }
      var count = { value: 0 };
      var blob = { value: null };
      tuple.getBlob(3, count, blob);
      do_check_eq(BLOB.length, count.value);
      for (var i = 0; i < BLOB.length; i++)
        do_check_eq(BLOB[i], blob.value[i]);
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_BLOB,
                  tuple.getTypeOfIndex(3));

      do_check_false(tuple.getIsNull(4));
      do_check_eq(tuple.getResultByName("id"), tuple.getResultByIndex(4));
      do_check_eq(INTEGER, tuple.getResultByName("id"));
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER,
                  tuple.getTypeOfIndex(4));

      // check that we have no more results
      tuple = aResultSet.getNextRow();
      do_check_eq(null, tuple);
    },
    handleError: function(aError)
    {
      print("error code " + aerror.result + " with message '" +
            aerror.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason + ") for test_get_data");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);
      do_check_true(this.resultObtained);

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_tuple_out_of_bounds()
{
  var stmt = getOpenedDatabase().createStatement(
    "SELECT string FROM test"
  );

  stmt.executeAsync({
    resultObtained: false,
    handleResult: function(aResultSet)
    {
      dump("handleResult("+aResultSet+");\n");
      do_check_false(this.resultObtained);
      this.resultObtained = true;

      // Check that we have a result
      var tuple = aResultSet.getNextRow();
      do_check_neq(null, tuple);

      // Check all out of bounds - should throw
      var methods = [
        "getTypeOfIndex",
        "getInt32",
        "getInt64",
        "getDouble",
        "getUTF8String",
        "getString",
        "getIsNull",
      ];
      for (var i in methods) {
        try {
          tuple[methods[i]](tuple.numEntries);
          do_throw("did not throw :(");
        }
        catch (e) {
          do_check_eq(Cr.NS_ERROR_ILLEGAL_VALUE, e.result);
        }
      }

      // getBlob requires more args...
      try {
        var blob = { value: null };
        var size = { value: 0 };
        tuple.getBlob(tuple.numEntries, blob, size);
        do_throw("did not throw :(");
      }
      catch (e) {
        do_check_eq(Cr.NS_ERROR_ILLEGAL_VALUE, e.result);
      }
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_tuple_out_of_bounds");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);
      do_check_true(this.resultObtained);

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_no_listener_works_on_success()
{
  var stmt = getOpenedDatabase().createStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindInt32Parameter(0, 0);
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_no_listener_works_on_results()
{
  var stmt = getOpenedDatabase().createStatement(
    "SELECT ?"
  );
  stmt.bindInt32Parameter(0, 1);
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_no_listener_works_on_error()
{
  // commit without a transaction will trigger an error
  var stmt = getOpenedDatabase().createStatement(
    "COMMIT"
  );
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_partial_listener_works()
{
  var stmt = getOpenedDatabase().createStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindInt32Parameter(0, 0);
  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
    }
  });
  stmt.executeAsync({
    handleError: function(aError)
    {
    }
  });
  stmt.executeAsync({
    handleCompletion: function(aReason)
    {
    }
  });
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_immediate_cancellation()
{
  var stmt = getOpenedDatabase().createStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindInt32Parameter(0, 0);
  var pendingStatement = stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      do_throw("unexpected result!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_immediate_cancellation");
      // It is possible that we finished before we canceled.
      do_check_true(aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED ||
                    aReason == Ci.mozIStorageStatementCallback.REASON_CANCELED);

      // Run the next test.
      run_next_test();
    }
  });

  // Cancel immediately
  pendingStatement.cancel()

  stmt.finalize();
}

function test_double_cancellation()
{
  var stmt = getOpenedDatabase().createStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindInt32Parameter(0, 0);
  var pendingStatement = stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      do_throw("unexpected result!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_double_cancellation");
      // It is possible that we finished before we canceled.
      do_check_true(aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED ||
                    aReason == Ci.mozIStorageStatementCallback.REASON_CANCELED);

      // Run the next test.
      run_next_test();
    }
  });

  // Cancel immediately
  pendingStatement.cancel()

  // And cancel again - expect an exception
  try {
    pendingStatement.cancel();
    do_throw("function call should have thrown!");
  }
  catch (e) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, e.result);
  }

  stmt.finalize();
}

function test_double_execute()
{
  var stmt = getOpenedDatabase().createStatement(
    "SELECT * FROM test"
  );

  var listener = {
    _timesCompleted: 0,
    _hasResults: false,
    handleResult: function(aResultSet)
    {
      do_check_false(this._hasResults);
      this._hasResults = true;
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_double_execute (iteration " +
            (this._timesCompleted + 1) + ")");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);
      do_check_true(this._hasResults);
      this._hasResults = false;
      this._timesCompleted++;

      // Run the next test.
      if (this._timesCompleted == 2)
        run_next_test();
    }
  }
  stmt.executeAsync(listener);
  stmt.executeAsync(listener);
  stmt.finalize();
}

function test_finalized_statement_does_not_crash()
{
  var stmt = getOpenedDatabase().createStatement(
    "SELECT * FROM TEST"
  );
  stmt.finalize();
  // we are concerned about a crash here; an error is fine.
  try {
    stmt.executeAsync();
  }
  catch (ex) {}

  // Run the next test.
  run_next_test();
}

function test_bind_multiple_rows_by_index()
{
  const AMOUNT_TO_ADD = 5;
  var stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (?, ?, ?, ?, ?)"
  );
  var array = stmt.newBindingParamsArray();
  for (let i = 0; i < AMOUNT_TO_ADD; i++) {
    let bp = array.newBindingParams();
    bp.bindByIndex(0, INTEGER);
    bp.bindByIndex(1, TEXT);
    bp.bindByIndex(2, REAL);
    bp.bindByIndex(3, null);
    bp.bindBlobByIndex(4, BLOB, BLOB.length);
    array.addParams(bp);
  }
  stmt.bindParameters(array);

  // Get our current number of rows in the table.
  var currentRows = 0;
  var countStmt = getOpenedDatabase().createStatement(
    "SELECT COUNT(1) AS count FROM test"
  );
  try {
    do_check_true(countStmt.executeStep());
    currentRows = countStmt.row.count;
    print("We have " + currentRows + " rows in test_bind_multiple_rows_by_index");
  }
  finally {
    countStmt.reset();
  }

  // Execute asynchronously.
  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      do_throw("Unexpected call to handleResult!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_bind_multiple_rows_by_index");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check to make sure we added all of our rows.
      try {
        do_check_true(countStmt.executeStep());
        print("We now have " + currentRows +
              " rows in test_bind_multiple_rows_by_index");
        do_check_eq(currentRows + AMOUNT_TO_ADD, countStmt.row.count);
      }
      finally {
        countStmt.finalize();
      }

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_bind_multiple_rows_by_name()
{
  const AMOUNT_TO_ADD = 5;
  var stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (:int, :text, :real, :null, :blob)"
  );
  var array = stmt.newBindingParamsArray();
  for (let i = 0; i < AMOUNT_TO_ADD; i++) {
    let bp = array.newBindingParams();
    bp.bindByName("int", INTEGER);
    bp.bindByName("text", TEXT);
    bp.bindByName("real", REAL);
    bp.bindByName("null", null);
    bp.bindBlobByName("blob", BLOB, BLOB.length);
    array.addParams(bp);
  }
  stmt.bindParameters(array);

  // Get our current number of rows in the table.
  var currentRows = 0;
  var countStmt = getOpenedDatabase().createStatement(
    "SELECT COUNT(1) AS count FROM test"
  );
  try {
    do_check_true(countStmt.executeStep());
    currentRows = countStmt.row.count;
    print("We have " + currentRows + " rows in test_bind_multiple_rows_by_name");
  }
  finally {
    countStmt.reset();
  }

  // Execute asynchronously.
  stmt.executeAsync({
    handleResult: function(aResultSet)
    {
      do_throw("Unexpected call to handleResult!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("unexpected error!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_bind_multiple_rows_by_name");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Check to make sure we added all of our rows.
      try {
        do_check_true(countStmt.executeStep());
        print("We now have " + currentRows +
              " rows in test_bind_multiple_rows_by_name");
        do_check_eq(currentRows + AMOUNT_TO_ADD, countStmt.row.count);
      }
      finally {
        countStmt.finalize();
      }

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_bind_out_of_bounds()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (?)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  // Check variant binding.
  let exceptionCaught = false;
  try {
    bp.bindByIndex(1, INTEGER);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Check blob binding.
  exceptionCaught = false;
  try {
    bp.bindBlobByIndex(1, BLOB, BLOB.length);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_bind_no_such_name()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:foo)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  // Check variant binding.
  let exceptionCaught = false;
  try {
    bp.bindByName("doesnotexist", INTEGER);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Check blob binding.
  exceptionCaught = false;
  try {
    bp.bindBlobByName("doesnotexist", BLOB, BLOB.length);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_bind_bogus_type_by_index()
{
  // We try to bind a JS Object here that should fail to bind.
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (blober) " +
    "VALUES (?)"
  );

  // We get an error after calling executeAsync, not when we bind.
  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  bp.bindByIndex(0, run_test);
  array.addParams(bp);
  stmt.bindParameters(array);

  stmt.executeAsync({
    _errorObtained: false,
    handleResult: function(aResultSet)
    {
      do_throw("Unexpected call to handleResult!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      this._errorObtained = true;
      do_check_eq(aError.result, Ci.mozIStorageError.MISMATCH);
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_bind_bogus_type_by_index");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_ERROR, aReason);
      do_check_true(this._errorObtained);

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_bind_bogus_type_by_name()
{
  // We try to bind a JS Object here that should fail to bind.
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (blober) " +
    "VALUES (:blob)"
  );

  // We get an error after calling executeAsync, not when we bind.
  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  bp.bindByName("blob", run_test);
  array.addParams(bp);
  stmt.bindParameters(array);

  stmt.executeAsync({
    _errorObtained: false,
    handleResult: function(aResultSet)
    {
      do_throw("Unexpected call to handleResult!");
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      this._errorObtained = true;
      do_check_eq(aError.result, Ci.mozIStorageError.MISMATCH);
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_bind_bogus_type_by_name");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_ERROR, aReason);
      do_check_true(this._errorObtained);

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

function test_bind_params_already_locked()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  bp.bindByName("int", INTEGER);
  array.addParams(bp);

  // We should get an error after we call addParams and try to bind again.
  let exceptionCaught = false;
  try {
    bp.bindByName("int", INTEGER);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Run the next test.
  run_next_test();
}

function test_bind_params_array_already_locked()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array = stmt.newBindingParamsArray();
  let bp1 = array.newBindingParams();
  bp1.bindByName("int", INTEGER);
  array.addParams(bp1);
  let bp2 = array.newBindingParams();
  stmt.bindParameters(array);
  bp2.bindByName("int", INTEGER);

  // We should get an error after we have bound the array to the statement.
  let exceptionCaught = false;
  try {
    array.addParams(bp2);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Run the next test.
  run_next_test();
}

function test_no_binding_params_from_locked_array()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  bp.bindByName("int", INTEGER);
  array.addParams(bp);
  stmt.bindParameters(array);

  // We should not be able to get a new BindingParams object after we have bound
  // to the statement.
  let exceptionCaught = false;
  try {
    bp = array.newBindingParams();
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Run the next test.
  run_next_test();
}

function test_not_right_owning_array()
{
  let stmt = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array1 = stmt.newBindingParamsArray();
  let array2 = stmt.newBindingParamsArray();
  let bp = array1.newBindingParams();
  bp.bindByName("int", INTEGER);

  // We should not be able to add bp to array2 since it was created from array1.
  let exceptionCaught = false;
  try {
    array2.addParams(bp);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Run the next test.
  run_next_test();
}

function test_not_right_owning_statement()
{
  let stmt1 = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );
  let stmt2 = getOpenedDatabase().createStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array1 = stmt1.newBindingParamsArray();
  let array2 = stmt2.newBindingParamsArray();
  let bp = array1.newBindingParams();
  bp.bindByName("int", INTEGER);
  array1.addParams(bp);

  // We should not be able to bind array1 since it was created from stmt1.
  let exceptionCaught = false;
  try {
    stmt2.bindParameters(array1);
    do_throw("we should have an exception!");
  }
  catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
    exceptionCaught = true;
  }
  do_check_true(exceptionCaught);

  // Run the next test.
  run_next_test();
}

function test_multiple_results()
{
  // First, we need to know how many rows we are expecting.
  let stmt = createStatement("SELECT COUNT(1) FROM test");
  try {
    do_check_true(stmt.executeStep());
    var expectedResults = stmt.getInt32(0);
  }
  finally {
    stmt.finalize();
  }

  // Sanity check - we should have more than one result, but let's be sure.
  do_check_true(expectedResults > 1);

  // Now check that we get back two rows of data from our async query.
  stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync({
    _results: 0,
    handleResult: function(aResultSet)
    {
      while (aResultSet.getNextRow())
        this._results++;
    },
    handleError: function(aError)
    {
      print("Error code " + aError.result + " with message '" +
            aError.message + "' returned.");
      do_throw("Unexpected call to handleError!");
    },
    handleCompletion: function(aReason)
    {
      print("handleCompletion(" + aReason +
            ") for test_multiple_results");
      do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason);

      // Make sure we have the right number of results.
      do_check_eq(this._results, expectedResults);

      // Run the next test.
      run_next_test();
    }
  });
  stmt.finalize();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

var tests =
[
  test_create_table,
  test_add_data,
  test_get_data,
  test_tuple_out_of_bounds,
  test_no_listener_works_on_success,
  test_no_listener_works_on_results,
  test_no_listener_works_on_error,
  test_partial_listener_works,
  test_immediate_cancellation,
  test_double_cancellation,
  test_double_execute,
  test_finalized_statement_does_not_crash,
  test_bind_multiple_rows_by_index,
  test_bind_multiple_rows_by_name,
  test_bind_out_of_bounds,
  test_bind_no_such_name,
  test_bind_bogus_type_by_index,
  test_bind_bogus_type_by_name,
  test_bind_params_already_locked,
  test_bind_params_array_already_locked,
  test_no_binding_params_from_locked_array,
  test_not_right_owning_array,
  test_not_right_owning_statement,
  test_multiple_results,
];
let index = 0;

function run_next_test()
{
  if (index < tests.length) {
    do_test_pending();
    print("Running the next test: " + tests[index].name);
    tests[index++]();
  }

  do_test_finished();
}

function run_test()
{
  cleanup();

  do_test_pending();
  run_next_test();
}
