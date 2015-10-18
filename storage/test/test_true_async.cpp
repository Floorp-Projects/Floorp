/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

////////////////////////////////////////////////////////////////////////////////
//// Tests

void
test_TrueAsyncStatement()
{
  // (only the first test needs to call this)
  hook_sqlite_mutex();

  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Start watching for forbidden mutex usage.
  watch_for_mutex_use_on_this_thread();

  // - statement with nothing to bind
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE test (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(stmt)
  );
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);

  // - statement with something to bind ordinally
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("INSERT INTO test (id) VALUES (?)"),
    getter_AddRefs(stmt)
  );
  stmt->BindInt32ByIndex(0, 1);
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);
  
  // - statement with something to bind by name
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("INSERT INTO test (id) VALUES (:id)"),
    getter_AddRefs(stmt)
  );
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  nsCOMPtr<mozIStorageBindingParams> params;
  paramsArray->NewBindingParams(getter_AddRefs(params));
  params->BindInt32ByName(NS_LITERAL_CSTRING("id"), 2);
  paramsArray->AddParams(params);
  params = nullptr;
  stmt->BindParameters(paramsArray);
  paramsArray = nullptr;
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(mutex_used_on_watched_thread);

  // - now, make sure creating a sync statement does trigger our guard.
  // (If this doesn't happen, our test is bunk and it's important to know that.)
  nsCOMPtr<mozIStorageStatement> syncStmt;
  db->CreateStatement(NS_LITERAL_CSTRING("SELECT * FROM test"),
                      getter_AddRefs(syncStmt));
  syncStmt->Finalize();
  do_check_true(mutex_used_on_watched_thread);

  blocking_async_close(db);
}

/**
 * Test that cancellation before a statement is run successfully stops the
 * statement from executing.
 */
void
test_AsyncCancellation()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- wedge the thread
  nsCOMPtr<nsIThread> target(get_conn_async_thread(db));
  do_check_true(target);
  RefPtr<ThreadWedger> wedger (new ThreadWedger(target));

  // -- create statements and cancel them
  // - async
  nsCOMPtr<mozIStorageAsyncStatement> asyncStmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE asyncTable (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(asyncStmt)
  );

  RefPtr<AsyncStatementSpinner> asyncSpin(new AsyncStatementSpinner());
  nsCOMPtr<mozIStoragePendingStatement> asyncPend;
  (void)asyncStmt->ExecuteAsync(asyncSpin, getter_AddRefs(asyncPend));
  do_check_true(asyncPend);
  asyncPend->Cancel();

  // - sync
  nsCOMPtr<mozIStorageStatement> syncStmt;
  db->CreateStatement(
    NS_LITERAL_CSTRING("CREATE TABLE syncTable (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(syncStmt)
  );

  RefPtr<AsyncStatementSpinner> syncSpin(new AsyncStatementSpinner());
  nsCOMPtr<mozIStoragePendingStatement> syncPend;
  (void)syncStmt->ExecuteAsync(syncSpin, getter_AddRefs(syncPend));
  do_check_true(syncPend);
  syncPend->Cancel();

  // -- unwedge the async thread
  wedger->unwedge();

  // -- verify that both statements report they were canceled
  asyncSpin->SpinUntilCompleted();
  do_check_true(asyncSpin->completionReason ==
                mozIStorageStatementCallback::REASON_CANCELED);

  syncSpin->SpinUntilCompleted();
  do_check_true(syncSpin->completionReason ==
                mozIStorageStatementCallback::REASON_CANCELED);

  // -- verify that neither statement constructed their tables
  nsresult rv;
  bool exists;
  rv = db->TableExists(NS_LITERAL_CSTRING("asyncTable"), &exists);
  do_check_true(rv == NS_OK);
  do_check_false(exists);
  rv = db->TableExists(NS_LITERAL_CSTRING("syncTable"), &exists);
  do_check_true(rv == NS_OK);
  do_check_false(exists);

  // -- cleanup
  asyncStmt->Finalize();
  syncStmt->Finalize();
  blocking_async_close(db);
}

/**
 * Test that the destructor for an asynchronous statement which has a
 *  sqlite3_stmt will dispatch that statement to the async thread for
 *  finalization rather than trying to finalize it on the main thread
 *  (and thereby running afoul of our mutex use detector).
 */
void test_AsyncDestructorFinalizesOnAsyncThread()
{
  // test_TrueAsyncStatement called hook_sqlite_mutex() for us

  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  watch_for_mutex_use_on_this_thread();

  // -- create an async statement
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(
    NS_LITERAL_CSTRING("CREATE TABLE test (id INTEGER PRIMARY KEY)"),
    getter_AddRefs(stmt)
  );

  // -- execute it so it gets a sqlite3_stmt that needs to be finalized
  blocking_async_execute(stmt);
  do_check_false(mutex_used_on_watched_thread);

  // -- forget our reference
  stmt = nullptr;

  // -- verify the mutex was not touched
  do_check_false(mutex_used_on_watched_thread);

  // -- make sure the statement actually gets finalized / cleanup
  // the close will assert if we failed to finalize!
  blocking_async_close(db);
}

void (*gTests[])(void) = {
  // this test must be first because it hooks the mutex mechanics
  test_TrueAsyncStatement,
  test_AsyncCancellation,
  test_AsyncDestructorFinalizesOnAsyncThread
};

const char *file = __FILE__;
#define TEST_NAME "true async statement"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
