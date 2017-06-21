/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Thorough branch coverage for asyncClose.
 *
 * Coverage of asyncClose by connection state at time of AsyncClose invocation:
 * - (asyncThread && mDBConn) => AsyncCloseConnection used, actually closes
 *   - test_asyncClose_does_not_complete_before_statements
 *   - test_double_asyncClose_throws
 *   - test_asyncClose_does_not_throw_without_callback
 * - (asyncThread && !mDBConn) => AsyncCloseConnection used, although no close
 *   is required.  Note that this is only possible in the event that
 *   openAsyncDatabase was used and we failed to open the database.
 *   Additionally, the async connection will never be exposed to the caller and
 *   AsyncInitDatabase will be the one to (automatically) call AsyncClose.
 *   - test_asyncClose_failed_open
 * - (!asyncThread && mDBConn) => Close() invoked, actually closes
 *   - test_asyncClose_on_sync_db
 * - (!asyncThread && !mDBConn) => Close() invoked, no close needed, errors.
 *   This happens if the database has already been closed.
 *   - test_double_asyncClose_throws
 */


/**
 * Sanity check that our close indeed happens after asynchronously executed
 * statements scheduled during the same turn of the event loop.  Note that we
 * just care that the statement says it completed without error, we're not
 * worried that the close will happen and then the statement will magically
 * complete.
 */
add_task(function* test_asyncClose_does_not_complete_before_statements() {
  let db = getService().openDatabase(getTestDB());
  let stmt = db.createStatement("SELECT * FROM sqlite_master");
  // Issue the executeAsync but don't yield for it...
  let asyncStatementPromise = executeAsync(stmt);
  stmt.finalize();

  // Issue the close.  (And now the order of yielding doesn't matter.)
  // Branch coverage: (asyncThread && mDBConn)
  yield asyncClose(db);
  equal((yield asyncStatementPromise),
        Ci.mozIStorageStatementCallback.REASON_FINISHED);
});

/**
 * Open an async database (ensures the async thread is created) and then invoke
 * AsyncClose() twice without yielding control flow.  The first will initiate
 * the actual async close after calling setClosedState which synchronously
 * impacts what the second call will observe.  The second call will then see the
 * async thread is not available and fall back to invoking Close() which will
 * notice the mDBConn is already gone.
 */
if (!AppConstants.DEBUG) {
  add_task(function* test_double_asyncClose_throws() {
    let db = yield openAsyncDatabase(getTestDB());

    // (Don't yield control flow yet, save the promise for after we make the
    // second call.)
    // Branch coverage: (asyncThread && mDBConn)
    let realClosePromise = yield asyncClose(db);
    try {
      // Branch coverage: (!asyncThread && !mDBConn)
      db.asyncClose();
      ok(false, "should have thrown");
    } catch (e) {
      equal(e.result, Cr.NS_ERROR_NOT_INITIALIZED);
    }

    yield realClosePromise;
  });
}

/**
 * Create a sync db connection and never take it asynchronous and then call
 * asyncClose on it.  This will bring the async thread to life to perform the
 * shutdown to avoid blocking the main thread, although we won't be able to
 * tell the difference between this happening and the method secretly shunting
 * to close().
 */
add_task(function* test_asyncClose_on_sync_db() {
  let db = getService().openDatabase(getTestDB());

  // Branch coverage: (!asyncThread && mDBConn)
  yield asyncClose(db);
  ok(true, "closed sync connection asynchronously");
});

/**
 * Fail to asynchronously open a DB in order to get an async thread existing
 * without having an open database and asyncClose invoked.  As per the file
 * doc-block, note that asyncClose will automatically be invoked by the
 * AsyncInitDatabase when it fails to open the database.  We will never be
 * provided with a reference to the connection and so cannot call AsyncClose on
 * it ourselves.
 */
add_task(function* test_asyncClose_failed_open() {
  // This will fail and the promise will be rejected.
  let openPromise = openAsyncDatabase(getFakeDB());
  yield openPromise.then(
    () => {
      ok(false, "we should have failed to open the db; this test is broken!");
    },
    () => {
      ok(true, "correctly failed to open db; bg asyncClose should happen");
    }
  );
  // (NB: we are unable to observe the thread shutdown, but since we never open
  // a database, this test is not going to interfere with other tests so much.)
});

// THE TEST BELOW WANTS TO BE THE LAST TEST WE RUN.  DO NOT MAKE IT SAD.
/**
 * Verify that asyncClose without a callback does not explode.  Without a
 * callback the shutdown is not actually observable, so we run this test last
 * in order to avoid weird overlaps.
 */
add_task(function* test_asyncClose_does_not_throw_without_callback() {
  let db = yield openAsyncDatabase(getTestDB());
  // Branch coverage: (asyncThread && mDBConn)
  db.asyncClose();
  ok(true, "if we shutdown cleanly and do not crash, then we succeeded");
});
// OBEY SHOUTING UPPER-CASE COMMENTS.
// ADD TESTS ABOVE THE FORMER TEST, NOT BELOW IT.
