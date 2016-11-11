/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests the functionality of
 * mozIStorageAsyncConnection::executeSimpleSQLAsync.
 */

const INTEGER = 1;
const TEXT = "this is test text";
const REAL = 3.23;

add_task(function* test_create_and_add() {
  let adb = yield openAsyncDatabase(getTestDB());

  let completion = yield executeSimpleSQLAsync(adb,
    "CREATE TABLE test (id INTEGER, string TEXT, number REAL)");

  do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);

  completion = yield executeSimpleSQLAsync(adb,
    "INSERT INTO test (id, string, number) " +
    "VALUES (" + INTEGER + ", \"" + TEXT + "\", " + REAL + ")");

  do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);

  let result = null;

  completion = yield executeSimpleSQLAsync(adb,
    "SELECT string, number FROM test WHERE id = 1",
    function(aResultSet) {
      result = aResultSet.getNextRow();
      do_check_eq(2, result.numEntries);
      do_check_eq(TEXT, result.getString(0));
      do_check_eq(REAL, result.getDouble(1));
    }
  );

  do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);
  do_check_neq(result, null);
  result = null;

  yield executeSimpleSQLAsync(adb, "SELECT COUNT(0) FROM test",
    function(aResultSet) {
      result = aResultSet.getNextRow();
      do_check_eq(1, result.getInt32(0));
    });

  do_check_neq(result, null);

  yield asyncClose(adb);
});


add_task(function* test_asyncClose_does_not_complete_before_statement() {
  let adb = yield openAsyncDatabase(getTestDB());
  let executed = false;

  let reason = yield executeSimpleSQLAsync(adb, "SELECT * FROM test",
    function(aResultSet) {
      let result = aResultSet.getNextRow();

      do_check_neq(result, null);
      do_check_eq(3, result.numEntries);
      do_check_eq(INTEGER, result.getInt32(0));
      do_check_eq(TEXT, result.getString(1));
      do_check_eq(REAL, result.getDouble(2));
      executed = true;
    }
  );

  do_check_eq(Ci.mozIStorageStatementCallback.REASON_FINISHED, reason);

  // Ensure that the statement executed to completion.
  do_check_true(executed);

  yield asyncClose(adb);
});
