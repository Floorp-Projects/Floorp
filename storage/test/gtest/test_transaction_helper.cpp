/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozStorageHelper.h"
#include "mozStorageConnection.h"

using namespace mozilla;
using namespace mozilla::storage;

bool has_transaction(mozIStorageConnection* aDB) {
  return !(static_cast<Connection*>(aDB)->getAutocommit());
}

/**
 * This file tests our Transaction helper in mozStorageHelper.h.
 */

TEST(storage_transaction_helper, Commit)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, call Commit, and make sure that it does
  // exists after the transaction falls out of scope.
  {
    mozStorageTransaction transaction(db, false);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);
    (void)transaction.Commit();
  }
  do_check_false(has_transaction(db));

  bool exists = false;
  (void)db->TableExists("test"_ns, &exists);
  do_check_true(exists);
}

TEST(storage_transaction_helper, Rollback)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, call Rollback, and make sure that it does
  // not exists after the transaction falls out of scope.
  {
    mozStorageTransaction transaction(db, true);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);
    (void)transaction.Rollback();
  }
  do_check_false(has_transaction(db));

  bool exists = true;
  (void)db->TableExists("test"_ns, &exists);
  do_check_false(exists);
}

TEST(storage_transaction_helper, AutoCommit)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, and make sure that it exists after the
  // transaction falls out of scope.  This means the Commit was successful.
  {
    mozStorageTransaction transaction(db, true);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);
  }
  do_check_false(has_transaction(db));

  bool exists = false;
  (void)db->TableExists("test"_ns, &exists);
  do_check_true(exists);
}

TEST(storage_transaction_helper, AutoRollback)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, and make sure that it does not exists
  // after the transaction falls out of scope.  This means the Rollback was
  // successful.
  {
    mozStorageTransaction transaction(db, false);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);
  }
  do_check_false(has_transaction(db));

  bool exists = true;
  (void)db->TableExists("test"_ns, &exists);
  do_check_false(exists);
}

TEST(storage_transaction_helper, null_database_connection)
{
  // We permit the use of the Transaction helper when passing a null database
  // in, so we need to make sure this still works without crashing.
  mozStorageTransaction transaction(nullptr, false);
  do_check_success(transaction.Start());
  do_check_true(NS_SUCCEEDED(transaction.Commit()));
  do_check_true(NS_SUCCEEDED(transaction.Rollback()));
}

TEST(storage_transaction_helper, async_Commit)
{
  HookSqliteMutex hook;

  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- wedge the thread
  nsCOMPtr<nsIThread> target(get_conn_async_thread(db));
  do_check_true(target);
  RefPtr<ThreadWedger> wedger(new ThreadWedger(target));

  {
    mozStorageTransaction transaction(
        db, false, mozIStorageConnection::TRANSACTION_DEFERRED, true);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);
    (void)transaction.Commit();
  }
  do_check_true(has_transaction(db));

  // -- unwedge the async thread
  wedger->unwedge();

  // Ensure the transaction has done its job by enqueueing an async execution.
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  (void)db->CreateAsyncStatement("SELECT NULL"_ns, getter_AddRefs(stmt));
  blocking_async_execute(stmt);
  stmt->Finalize();
  do_check_false(has_transaction(db));
  bool exists = false;
  (void)db->TableExists("test"_ns, &exists);
  do_check_true(exists);

  blocking_async_close(db);
}

TEST(storage_transaction_helper, Nesting)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  {
    mozStorageTransaction transaction(db, false);
    do_check_success(transaction.Start());
    do_check_true(has_transaction(db));
    do_check_success(
        db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns));

    {
      mozStorageTransaction nestedTransaction(db, false);
      do_check_success(nestedTransaction.Start());
      do_check_true(has_transaction(db));
      do_check_success(db->ExecuteSimpleSQL(
          "CREATE TABLE nested_test (id INTEGER PRIMARY KEY)"_ns));

#ifndef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      do_check_true(transaction.Commit() == NS_ERROR_NOT_AVAILABLE);
      do_check_true(transaction.Rollback() == NS_ERROR_NOT_AVAILABLE);
#endif
    }

    bool exists = false;
    do_check_success(db->TableExists("nested_test"_ns, &exists));
    do_check_false(exists);

    (void)transaction.Commit();
  }
  do_check_false(has_transaction(db));

  bool exists = false;
  do_check_success(db->TableExists("test"_ns, &exists));
  do_check_true(exists);
}
