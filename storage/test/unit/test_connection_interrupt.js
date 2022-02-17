/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests the functionality of mozIStorageAsyncConnection::interrupt
// in the asynchronous case.
add_task(async function test_wr_async_conn() {
  // Interrupt cannot be used on R/W async connections.
  let db = await openAsyncDatabase(getTestDB());
  await db.interrupt();
  info("should be able to interrupt a R/W async connection");
  await asyncClose(db);
});

add_task(async function test_closed_conn() {
  let db = await openAsyncDatabase(getTestDB(), { readOnly: true });
  await asyncClose(db);
  Assert.throws(
    () => db.interrupt(),
    /NS_ERROR_NOT_INITIALIZED/,
    "interrupt() should throw if invoked on a closed connection"
  );
});

add_task(
  {
    // We use a timeout in the test that may be insufficient on Android emulators.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_async_conn() {
    let db = await openAsyncDatabase(getTestDB(), { readOnly: true });
    // This query is built to hang forever.
    let stmt = db.createAsyncStatement(`
    WITH RECURSIVE test(n) AS (
      VALUES(1)
      UNION ALL
      SELECT n + 1 FROM test
    )
    SELECT t.n
    FROM test,test AS t`);

    let completePromise = new Promise((resolve, reject) => {
      let listener = {
        handleResult(aResultSet) {
          reject();
        },
        handleError(aError) {
          reject();
        },
        handleCompletion(aReason) {
          resolve(aReason);
        },
      };
      stmt.executeAsync(listener);
      stmt.finalize();
    });

    // Wait for the statement to be executing.
    // This is not rock-solid, see the discussion in bug 1320301. A better
    // approach will be evaluated in a separate bug.
    await new Promise(resolve => do_timeout(500, resolve));

    db.interrupt();

    Assert.equal(
      await completePromise,
      Ci.mozIStorageStatementCallback.REASON_CANCELED,
      "Should have been canceled"
    );

    await asyncClose(db);
  }
);

add_task(
  {
    // We use a timeout in the test that may be insufficient on Android emulators.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_async_conn() {
    let db = await openAsyncDatabase(getTestDB());
    // This query is built to hang forever.
    let stmt = db.createAsyncStatement(`
    WITH RECURSIVE test(n) AS (
      VALUES(1)
      UNION ALL
      SELECT n + 1 FROM test
    )
    SELECT t.n
    FROM test,test AS t`);

    let completePromise = new Promise((resolve, reject) => {
      let listener = {
        handleResult(aResultSet) {
          reject();
        },
        handleError(aError) {
          reject();
        },
        handleCompletion(aReason) {
          resolve(aReason);
        },
      };
      stmt.executeAsync(listener);
      stmt.finalize();
    });

    // Wait for the statement to be executing.
    // This is not rock-solid, see the discussion in bug 1320301. A better
    // approach will be evaluated in a separate bug.
    await new Promise(resolve => do_timeout(500, resolve));

    // We are going to interrupt a database connection
    db.interrupt();

    Assert.equal(
      await completePromise,
      Ci.mozIStorageStatementCallback.REASON_CANCELED,
      "Should have been able to cancel even for R/W database"
    );

    await asyncClose(db);
  }
);
