/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests the functionality of mozIStorageBaseStatement::executeAsync
 * for both mozIStorageStatement and mozIStorageAsyncStatement.
 */

const INTEGER = 1;
const TEXT = "this is test text";
const REAL = 3.23;
const BLOB = [1, 2];

/**
 * Execute the given statement asynchronously, spinning an event loop until the
 * async statement completes.
 *
 * @param aStmt
 *        The statement to execute.
 * @param [aOptions={}]
 * @param [aOptions.error=false]
 *        If true we should expect an error whose code we do not care about.  If
 *        a numeric value, that's the error code we expect and require.  If we
 *        are expecting an error, we expect a completion reason of REASON_ERROR.
 *        Otherwise we expect no error notification and a completion reason of
 *        REASON_FINISHED.
 * @param [aOptions.cancel]
 *        If true we cancel the pending statement and additionally return the
 *        pending statement in case you want to further manipulate it.
 * @param [aOptions.returnPending=false]
 *        If true we keep the pending statement around and return it to you.  We
 *        normally avoid doing this to try and minimize the amount of time a
 *        reference is held to the returned pending statement.
 * @param [aResults]
 *        If omitted, we assume no results rows are expected.  If it is a
 *        number, we assume it is the number of results rows expected.  If it is
 *        a function, we assume it is a function that takes the 1) result row
 *        number, 2) result tuple, 3) call stack for the original call to
 *        execAsync as arguments.  If it is a list, we currently assume it is a
 *        list of functions where each function is intended to evaluate the
 *        result row at that ordinal position and takes the result tuple and
 *        the call stack for the original call.
 */
function execAsync(aStmt, aOptions, aResults)
{
  let caller = Components.stack.caller;
  if (aOptions == null)
    aOptions = {};

  let resultsExpected;
  let resultsChecker;
  if (aResults == null) {
    resultsExpected = 0;
  }
  else if (typeof aResults == "number") {
    resultsExpected = aResults;
  }
  else if (typeof aResults == "function") {
    resultsChecker = aResults;
  }
  else { // array
    resultsExpected = aResults.length;
    resultsChecker = function(aResultNum, aTup, aCaller) {
      aResults[aResultNum](aTup, aCaller);
    };
  }
  let resultsSeen = 0;

  let errorCodeExpected = false;
  let reasonExpected = Ci.mozIStorageStatementCallback.REASON_FINISHED;
  let altReasonExpected = null;
  if ("error" in aOptions) {
    errorCodeExpected = aOptions.error;
    if (errorCodeExpected)
      reasonExpected = Ci.mozIStorageStatementCallback.REASON_ERROR;
  }
  let errorCodeSeen = false;

  if ("cancel" in aOptions && aOptions.cancel)
    altReasonExpected = Ci.mozIStorageStatementCallback.REASON_CANCELED;

  let completed = false;

  let listener = {
    handleResult(aResultSet)
    {
      let row, resultsSeenThisCall = 0;
      while ((row = aResultSet.getNextRow()) != null) {
        if (resultsChecker)
          resultsChecker(resultsSeen, row, caller);
        resultsSeen++;
        resultsSeenThisCall++;
      }

      if (!resultsSeenThisCall)
        do_throw("handleResult invoked with 0 result rows!");
    },
    handleError(aError)
    {
      if (errorCodeSeen != false)
        do_throw("handleError called when we already had an error!");
      errorCodeSeen = aError.result;
    },
    handleCompletion(aReason)
    {
      if (completed) // paranoia check
        do_throw("Received a second handleCompletion notification!", caller);

      if (resultsSeen != resultsExpected)
        do_throw("Expected " + resultsExpected + " rows of results but " +
                 "got " + resultsSeen + " rows!", caller);

      if (errorCodeExpected == true && errorCodeSeen == false)
        do_throw("Expected an error, but did not see one.", caller);
      else if (errorCodeExpected != errorCodeSeen)
        do_throw("Expected error code " + errorCodeExpected + " but got " +
                 errorCodeSeen, caller);

      if (aReason != reasonExpected && aReason != altReasonExpected)
        do_throw("Expected reason " + reasonExpected +
                 (altReasonExpected ? (" or " + altReasonExpected) : "") +
                 " but got " + aReason, caller);

      completed = true;
    }
  };

  let pending;
  // Only get a pending reference if we're supposed to do.
  // (note: This does not stop XPConnect from holding onto one currently.)
  if (("cancel" in aOptions && aOptions.cancel) ||
      ("returnPending" in aOptions && aOptions.returnPending)) {
    pending = aStmt.executeAsync(listener);
  }
  else {
    aStmt.executeAsync(listener);
  }

  if ("cancel" in aOptions && aOptions.cancel)
    pending.cancel();

  let curThread = Components.classes["@mozilla.org/thread-manager;1"]
                            .getService().currentThread;
  while (!completed && !_quit)
    curThread.processNextEvent(true);

  return pending;
}

/**
 * Make sure that illegal SQL generates the expected runtime error and does not
 * result in any crashes.  Async-only since the synchronous case generates the
 * error synchronously (and is tested elsewhere).
 */
function test_illegal_sql_async_deferred()
{
  // gibberish
  let stmt = makeTestStatement("I AM A ROBOT. DO AS I SAY.");
  execAsync(stmt, {error: Ci.mozIStorageError.ERROR});
  stmt.finalize();

  // legal SQL syntax, but with semantics issues.
  stmt = makeTestStatement("SELECT destination FROM funkytown");
  execAsync(stmt, {error: Ci.mozIStorageError.ERROR});
  stmt.finalize();

  run_next_test();
}
test_illegal_sql_async_deferred.asyncOnly = true;

function test_create_table()
{
  // Ensure our table doesn't exist
  do_check_false(getOpenedDatabase().tableExists("test"));

  var stmt = makeTestStatement(
    "CREATE TABLE test (" +
      "id INTEGER, " +
      "string TEXT, " +
      "number REAL, " +
      "nuller NULL, " +
      "blober BLOB" +
    ")"
  );
  execAsync(stmt);
  stmt.finalize();

  // Check that the table has been created
  do_check_true(getOpenedDatabase().tableExists("test"));

  // Verify that it's created correctly (this will throw if it wasn't)
  let checkStmt = getOpenedDatabase().createStatement(
    "SELECT id, string, number, nuller, blober FROM test"
  );
  checkStmt.finalize();
  run_next_test();
}

function test_add_data()
{
  var stmt = makeTestStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (?, ?, ?, ?, ?)"
  );
  stmt.bindBlobByIndex(4, BLOB, BLOB.length);
  stmt.bindByIndex(3, null);
  stmt.bindByIndex(2, REAL);
  stmt.bindByIndex(1, TEXT);
  stmt.bindByIndex(0, INTEGER);

  execAsync(stmt);
  stmt.finalize();

  // Check that the result is in the table
  verifyQuery("SELECT string, number, nuller, blober FROM test WHERE id = ?",
              INTEGER,
              [TEXT, REAL, null, BLOB]);
  run_next_test();
}

function test_get_data()
{
  var stmt = makeTestStatement(
    "SELECT string, number, nuller, blober, id FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, INTEGER);
  execAsync(stmt, {}, [
    function(tuple) {
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
      for (let i = 0; i < BLOB.length; i++) {
        do_check_eq(BLOB[i], blobByName[i]);
        do_check_eq(BLOB[i], blobByIndex[i]);
      }
      var count = { value: 0 };
      var blob = { value: null };
      tuple.getBlob(3, count, blob);
      do_check_eq(BLOB.length, count.value);
      for (let i = 0; i < BLOB.length; i++)
        do_check_eq(BLOB[i], blob.value[i]);
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_BLOB,
                  tuple.getTypeOfIndex(3));

      do_check_false(tuple.getIsNull(4));
      do_check_eq(tuple.getResultByName("id"), tuple.getResultByIndex(4));
      do_check_eq(INTEGER, tuple.getResultByName("id"));
      do_check_eq(Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER,
                  tuple.getTypeOfIndex(4));
    }]);
  stmt.finalize();
  run_next_test();
}

function test_tuple_out_of_bounds()
{
  var stmt = makeTestStatement(
    "SELECT string FROM test"
  );
  execAsync(stmt, {}, [
    function(tuple) {
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
    }]);
  stmt.finalize();
  run_next_test();
}

function test_no_listener_works_on_success()
{
  var stmt = makeTestStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, 0);
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_no_listener_works_on_results()
{
  var stmt = makeTestStatement(
    "SELECT ?"
  );
  stmt.bindByIndex(0, 1);
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_no_listener_works_on_error()
{
  // commit without a transaction will trigger an error
  var stmt = makeTestStatement(
    "COMMIT"
  );
  stmt.executeAsync();
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

function test_partial_listener_works()
{
  var stmt = makeTestStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, 0);
  stmt.executeAsync({
    handleResult(aResultSet) {}
  });
  stmt.executeAsync({
    handleError(aError) {}
  });
  stmt.executeAsync({
    handleCompletion(aReason) {}
  });
  stmt.finalize();

  // Run the next test.
  run_next_test();
}

/**
 * Dubious cancellation test that depends on system loading may or may not
 * succeed in canceling things.  It does at least test if calling cancel blows
 * up.  test_AsyncCancellation in test_true_async.cpp is our test that canceling
 * actually works correctly.
 */
function test_immediate_cancellation()
{
  var stmt = makeTestStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, 0);
  execAsync(stmt, {cancel: true});
  stmt.finalize();
  run_next_test();
}

/**
 * Test that calling cancel twice throws the second time.
 */
function test_double_cancellation()
{
  var stmt = makeTestStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, 0);
  let pendingStatement = execAsync(stmt, {cancel: true});
  // And cancel again - expect an exception
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => pendingStatement.cancel());

  stmt.finalize();
  run_next_test();
}

/**
 * Verify that nothing untoward happens if we try and cancel something after it
 * has fully run to completion.
 */
function test_cancellation_after_execution()
{
  var stmt = makeTestStatement(
    "DELETE FROM test WHERE id = ?"
  );
  stmt.bindByIndex(0, 0);
  let pendingStatement = execAsync(stmt, {returnPending: true});
  // (the statement has fully executed at this point)
  // canceling after the statement has run to completion should not throw!
  pendingStatement.cancel();

  stmt.finalize();
  run_next_test();
}

/**
 * Verifies that a single statement can be executed more than once.  Might once
 * have been intended to also ensure that callback notifications were not
 * incorrectly interleaved, but that part was brittle (it's totally fine for
 * handleResult to get called multiple times) and not comprehensive.
 */
function test_double_execute()
{
  var stmt = makeTestStatement(
    "SELECT 1"
  );
  execAsync(stmt, null, 1);
  execAsync(stmt, null, 1);
  stmt.finalize();
  run_next_test();
}

function test_finalized_statement_does_not_crash()
{
  var stmt = makeTestStatement(
    "SELECT * FROM TEST"
  );
  stmt.finalize();
  // we are concerned about a crash here; an error is fine.
  try {
    stmt.executeAsync();
  } catch (ex) {
    // Do nothing.
  }

  // Run the next test.
  run_next_test();
}

/**
 * Bind by mozIStorageBindingParams on the mozIStorageBaseStatement by index.
 */
function test_bind_direct_binding_params_by_index()
{
  var stmt = makeTestStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (?, ?, ?, ?, ?)"
  );
  let insertId = nextUniqueId++;
  stmt.bindByIndex(0, insertId);
  stmt.bindByIndex(1, TEXT);
  stmt.bindByIndex(2, REAL);
  stmt.bindByIndex(3, null);
  stmt.bindBlobByIndex(4, BLOB, BLOB.length);
  execAsync(stmt);
  stmt.finalize();
  verifyQuery("SELECT string, number, nuller, blober FROM test WHERE id = ?",
              insertId,
              [TEXT, REAL, null, BLOB]);
  run_next_test();
}

/**
 * Bind by mozIStorageBindingParams on the mozIStorageBaseStatement by name.
 */
function test_bind_direct_binding_params_by_name()
{
  var stmt = makeTestStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (:int, :text, :real, :null, :blob)"
  );
  let insertId = nextUniqueId++;
  stmt.bindByName("int", insertId);
  stmt.bindByName("text", TEXT);
  stmt.bindByName("real", REAL);
  stmt.bindByName("null", null);
  stmt.bindBlobByName("blob", BLOB, BLOB.length);
  execAsync(stmt);
  stmt.finalize();
  verifyQuery("SELECT string, number, nuller, blober FROM test WHERE id = ?",
              insertId,
              [TEXT, REAL, null, BLOB]);
  run_next_test();
}

function test_bind_js_params_helper_by_index()
{
  var stmt = makeTestStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (?, ?, ?, ?, NULL)"
  );
  let insertId = nextUniqueId++;
  // we cannot bind blobs this way; no blober
  stmt.params[3] = null;
  stmt.params[2] = REAL;
  stmt.params[1] = TEXT;
  stmt.params[0] = insertId;
  execAsync(stmt);
  stmt.finalize();
  verifyQuery("SELECT string, number, nuller FROM test WHERE id = ?", insertId,
              [TEXT, REAL, null]);
  run_next_test();
}

function test_bind_js_params_helper_by_name()
{
  var stmt = makeTestStatement(
    "INSERT INTO test (id, string, number, nuller, blober) " +
    "VALUES (:int, :text, :real, :null, NULL)"
  );
  let insertId = nextUniqueId++;
  // we cannot bind blobs this way; no blober
  stmt.params.null = null;
  stmt.params.real = REAL;
  stmt.params.text = TEXT;
  stmt.params.int = insertId;
  execAsync(stmt);
  stmt.finalize();
  verifyQuery("SELECT string, number, nuller FROM test WHERE id = ?", insertId,
              [TEXT, REAL, null]);
  run_next_test();
}

function test_bind_multiple_rows_by_index()
{
  const AMOUNT_TO_ADD = 5;
  var stmt = makeTestStatement(
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
    do_check_eq(array.length, i + 1);
  }
  stmt.bindParameters(array);

  let rowCount = getTableRowCount("test");
  execAsync(stmt);
  do_check_eq(rowCount + AMOUNT_TO_ADD, getTableRowCount("test"));
  stmt.finalize();
  run_next_test();
}

function test_bind_multiple_rows_by_name()
{
  const AMOUNT_TO_ADD = 5;
  var stmt = makeTestStatement(
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
    do_check_eq(array.length, i + 1);
  }
  stmt.bindParameters(array);

  let rowCount = getTableRowCount("test");
  execAsync(stmt);
  do_check_eq(rowCount + AMOUNT_TO_ADD, getTableRowCount("test"));
  stmt.finalize();
  run_next_test();
}

/**
 * Verify that a mozIStorageStatement instance throws immediately when we
 * try and bind to an illegal index.
 */
function test_bind_out_of_bounds_sync_immediate()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (?)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  // Check variant binding.
  expectError(Cr.NS_ERROR_INVALID_ARG,
              () => bp.bindByIndex(1, INTEGER));
  // Check blob binding.
  expectError(Cr.NS_ERROR_INVALID_ARG,
              () => bp.bindBlobByIndex(1, BLOB, BLOB.length));

  stmt.finalize();
  run_next_test();
}
test_bind_out_of_bounds_sync_immediate.syncOnly = true;

/**
 * Verify that a mozIStorageAsyncStatement reports an error asynchronously when
 * we bind to an illegal index.
 */
function test_bind_out_of_bounds_async_deferred()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (?)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  // There is no difference between variant and blob binding for async purposes.
  bp.bindByIndex(1, INTEGER);
  array.addParams(bp);
  stmt.bindParameters(array);
  execAsync(stmt, {error: Ci.mozIStorageError.RANGE});

  stmt.finalize();
  run_next_test();
}
test_bind_out_of_bounds_async_deferred.asyncOnly = true;

function test_bind_no_such_name_sync_immediate()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:foo)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  // Check variant binding.
  expectError(Cr.NS_ERROR_INVALID_ARG,
              () => bp.bindByName("doesnotexist", INTEGER));
  // Check blob binding.
  expectError(Cr.NS_ERROR_INVALID_ARG,
              () => bp.bindBlobByName("doesnotexist", BLOB, BLOB.length));

  stmt.finalize();
  run_next_test();
}
test_bind_no_such_name_sync_immediate.syncOnly = true;

function test_bind_no_such_name_async_deferred()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:foo)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();

  bp.bindByName("doesnotexist", INTEGER);
  array.addParams(bp);
  stmt.bindParameters(array);
  execAsync(stmt, {error: Ci.mozIStorageError.RANGE});

  stmt.finalize();
  run_next_test();
}
test_bind_no_such_name_async_deferred.asyncOnly = true;

function test_bind_bogus_type_by_index()
{
  // We try to bind a JS Object here that should fail to bind.
  let stmt = makeTestStatement(
    "INSERT INTO test (blober) " +
    "VALUES (?)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  Assert.throws(() => bp.bindByIndex(0, run_test), /NS_ERROR_UNEXPECTED/);

  stmt.finalize();
  run_next_test();
}

function test_bind_bogus_type_by_name()
{
  // We try to bind a JS Object here that should fail to bind.
  let stmt = makeTestStatement(
    "INSERT INTO test (blober) " +
    "VALUES (:blob)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  Assert.throws(() => bp.bindByName("blob", run_test), /NS_ERROR_UNEXPECTED/);

  stmt.finalize();
  run_next_test();
}

function test_bind_params_already_locked()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array = stmt.newBindingParamsArray();
  let bp = array.newBindingParams();
  bp.bindByName("int", INTEGER);
  array.addParams(bp);

  // We should get an error after we call addParams and try to bind again.
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => bp.bindByName("int", INTEGER));

  stmt.finalize();
  run_next_test();
}

function test_bind_params_array_already_locked()
{
  let stmt = makeTestStatement(
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
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => array.addParams(bp2));

  stmt.finalize();
  run_next_test();
}

function test_no_binding_params_from_locked_array()
{
  let stmt = makeTestStatement(
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
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => array.newBindingParams());

  stmt.finalize();
  run_next_test();
}

function test_not_right_owning_array()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array1 = stmt.newBindingParamsArray();
  let array2 = stmt.newBindingParamsArray();
  let bp = array1.newBindingParams();
  bp.bindByName("int", INTEGER);

  // We should not be able to add bp to array2 since it was created from array1.
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => array2.addParams(bp));

  stmt.finalize();
  run_next_test();
}

function test_not_right_owning_statement()
{
  let stmt1 = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );
  let stmt2 = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let array1 = stmt1.newBindingParamsArray();
  stmt2.newBindingParamsArray();
  let bp = array1.newBindingParams();
  bp.bindByName("int", INTEGER);
  array1.addParams(bp);

  // We should not be able to bind array1 since it was created from stmt1.
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => stmt2.bindParameters(array1));

  stmt1.finalize();
  stmt2.finalize();
  run_next_test();
}

function test_bind_empty_array()
{
  let stmt = makeTestStatement(
    "INSERT INTO test (id) " +
    "VALUES (:int)"
  );

  let paramsArray = stmt.newBindingParamsArray();

  // We should not be able to bind this array to the statement because it is
  // empty.
  expectError(Cr.NS_ERROR_UNEXPECTED,
              () => stmt.bindParameters(paramsArray));

  stmt.finalize();
  run_next_test();
}

function test_multiple_results()
{
  let expectedResults = getTableRowCount("test");
  // Sanity check - we should have more than one result, but let's be sure.
  do_check_true(expectedResults > 1);

  // Now check that we get back two rows of data from our async query.
  let stmt = makeTestStatement("SELECT * FROM test");
  execAsync(stmt, {}, expectedResults);

  stmt.finalize();
  run_next_test();
}

// Test Runner

const TEST_PASS_SYNC = 0;
const TEST_PASS_ASYNC = 1;
/**
 * We run 2 passes against the test.  One where makeTestStatement generates
 * synchronous (mozIStorageStatement) statements and one where it generates
 * asynchronous (mozIStorageAsyncStatement) statements.
 *
 * Because of differences in the ability to know the number of parameters before
 * dispatching, some tests are sync/async specific.  These functions are marked
 * with 'syncOnly' or 'asyncOnly' attributes and run_next_test knows what to do.
 */
var testPass = TEST_PASS_SYNC;

/**
 * Create a statement of the type under test per testPass.
 *
 * @param aSQL
 *        The SQL string from which to build a statement.
 * @return a statement of the type under test per testPass.
 */
function makeTestStatement(aSQL) {
  if (testPass == TEST_PASS_SYNC) {
    return getOpenedDatabase().createStatement(aSQL);
  }
  return getOpenedDatabase().createAsyncStatement(aSQL);
}

var tests = [
  test_illegal_sql_async_deferred,
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
  test_cancellation_after_execution,
  test_double_execute,
  test_finalized_statement_does_not_crash,
  test_bind_direct_binding_params_by_index,
  test_bind_direct_binding_params_by_name,
  test_bind_js_params_helper_by_index,
  test_bind_js_params_helper_by_name,
  test_bind_multiple_rows_by_index,
  test_bind_multiple_rows_by_name,
  test_bind_out_of_bounds_sync_immediate,
  test_bind_out_of_bounds_async_deferred,
  test_bind_no_such_name_sync_immediate,
  test_bind_no_such_name_async_deferred,
  test_bind_bogus_type_by_index,
  test_bind_bogus_type_by_name,
  test_bind_params_already_locked,
  test_bind_params_array_already_locked,
  test_bind_empty_array,
  test_no_binding_params_from_locked_array,
  test_not_right_owning_array,
  test_not_right_owning_statement,
  test_multiple_results,
];
var index = 0;

const STARTING_UNIQUE_ID = 2;
var nextUniqueId = STARTING_UNIQUE_ID;

function run_next_test()
{
  function _run_next_test() {
    // use a loop so we can skip tests...
    while (index < tests.length) {
      let test = tests[index++];
      // skip tests not appropriate to the current test pass
      if ((testPass == TEST_PASS_SYNC && ("asyncOnly" in test)) ||
          (testPass == TEST_PASS_ASYNC && ("syncOnly" in test)))
        continue;

      // Asynchronous tests means that exceptions don't kill the test.
      try {
        print("****** Running the next test: " + test.name);
        test();
        return;
      }
      catch (e) {
        do_throw(e);
      }
    }

    // if we only completed the first pass, move to the next pass
    if (testPass == TEST_PASS_SYNC) {
      print("********* Beginning mozIStorageAsyncStatement pass.");
      testPass++;
      index = 0;
      // a new pass demands a new database
      asyncCleanup();
      nextUniqueId = STARTING_UNIQUE_ID;
      _run_next_test();
      return;
    }

    // we did some async stuff; we need to clean up.
    asyncCleanup();
    do_test_finished();
  }

  // Don't actually schedule another test if we're quitting.
  if (!_quit) {
    // For saner stacks, we execute this code RSN.
    do_execute_soon(_run_next_test);
  }
}

function run_test()
{
  cleanup();

  do_test_pending();
  run_next_test();
}
