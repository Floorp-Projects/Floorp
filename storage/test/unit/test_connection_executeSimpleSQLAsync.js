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

add_task(async function test_create_and_add() {
  let adb = await openAsyncDatabase(getTestDB());

  let completion = await executeSimpleSQLAsync(adb,
    "CREATE TABLE test (id INTEGER, string TEXT, number REAL)");

  Assert.equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);

  completion = await executeSimpleSQLAsync(adb,
    "INSERT INTO test (id, string, number) " +
    "VALUES (" + INTEGER + ", \"" + TEXT + "\", " + REAL + ")");

  Assert.equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);

  let result = null;

  completion = await executeSimpleSQLAsync(adb,
    "SELECT string, number FROM test WHERE id = 1",
    function(aResultSet) {
      result = aResultSet.getNextRow();
      Assert.equal(2, result.numEntries);
      Assert.equal(TEXT, result.getString(0));
      Assert.equal(REAL, result.getDouble(1));
    }
  );

  Assert.equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, completion);
  Assert.notEqual(result, null);
  result = null;

  await executeSimpleSQLAsync(adb, "SELECT COUNT(0) FROM test",
    function(aResultSet) {
      result = aResultSet.getNextRow();
      Assert.equal(1, result.getInt32(0));
    });

  Assert.notEqual(result, null);

  await asyncClose(adb);
});


add_task(async function test_asyncClose_does_not_complete_before_statement() {
  let adb = await openAsyncDatabase(getTestDB());
  let executed = false;

  let reason = await executeSimpleSQLAsync(adb, "SELECT * FROM test",
    function(aResultSet) {
      let result = aResultSet.getNextRow();

      Assert.notEqual(result, null);
      Assert.equal(3, result.numEntries);
      Assert.equal(INTEGER, result.getInt32(0));
      Assert.equal(TEXT, result.getString(1));
      Assert.equal(REAL, result.getDouble(2));
      executed = true;
    }
  );

  Assert.equal(Ci.mozIStorageStatementCallback.REASON_FINISHED, reason);

  // Ensure that the statement executed to completion.
  Assert.ok(executed);

  await asyncClose(adb);
});
