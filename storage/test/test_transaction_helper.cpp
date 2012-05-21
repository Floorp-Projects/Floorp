/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozStorageHelper.h"

/**
 * This file test our Transaction helper in mozStorageHelper.h.
 */

void
test_HasTransaction()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // First test that it holds the transaction after it should have gotten one.
  {
    mozStorageTransaction transaction(db, false);
    do_check_true(transaction.HasTransaction());
    (void)transaction.Commit();
    // And that it does not have a transaction after we have committed.
    do_check_false(transaction.HasTransaction());
  }

  // Check that no transaction is had after a rollback.
  {
    mozStorageTransaction transaction(db, false);
    do_check_true(transaction.HasTransaction());
    (void)transaction.Rollback();
    do_check_false(transaction.HasTransaction());
  }

  // Check that we do not have a transaction if one is already obtained.
  mozStorageTransaction outerTransaction(db, false);
  do_check_true(outerTransaction.HasTransaction());
  {
    mozStorageTransaction innerTransaction(db, false);
    do_check_false(innerTransaction.HasTransaction());
  }
}

void
test_Commit()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, call Commit, and make sure that it does
  // exists after the transaction falls out of scope.
  {
    mozStorageTransaction transaction(db, false);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test (id INTEGER PRIMARY KEY)"
    ));
    (void)transaction.Commit();
  }

  bool exists = false;
  (void)db->TableExists(NS_LITERAL_CSTRING("test"), &exists);
  do_check_true(exists);
}

void
test_Rollback()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, call Rollback, and make sure that it does
  // not exists after the transaction falls out of scope.
  {
    mozStorageTransaction transaction(db, true);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test (id INTEGER PRIMARY KEY)"
    ));
    (void)transaction.Rollback();
  }

  bool exists = true;
  (void)db->TableExists(NS_LITERAL_CSTRING("test"), &exists);
  do_check_false(exists);
}

void
test_AutoCommit()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, and make sure that it exists after the
  // transaction falls out of scope.  This means the Commit was successful.
  {
    mozStorageTransaction transaction(db, true);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test (id INTEGER PRIMARY KEY)"
    ));
  }

  bool exists = false;
  (void)db->TableExists(NS_LITERAL_CSTRING("test"), &exists);
  do_check_true(exists);
}

void
test_AutoRollback()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a table in a transaction, and make sure that it does not exists
  // after the transaction falls out of scope.  This means the Rollback was
  // successful.
  {
    mozStorageTransaction transaction(db, false);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test (id INTEGER PRIMARY KEY)"
    ));
  }

  bool exists = true;
  (void)db->TableExists(NS_LITERAL_CSTRING("test"), &exists);
  do_check_false(exists);
}

void
test_SetDefaultAction()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // First we test that rollback happens when we first set it to automatically
  // commit.
  {
    mozStorageTransaction transaction(db, true);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test1 (id INTEGER PRIMARY KEY)"
    ));
    transaction.SetDefaultAction(false);
  }
  bool exists = true;
  (void)db->TableExists(NS_LITERAL_CSTRING("test1"), &exists);
  do_check_false(exists);

  // Now we do the opposite and test that a commit happens when we first set it
  // to automatically rollback.
  {
    mozStorageTransaction transaction(db, false);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test2 (id INTEGER PRIMARY KEY)"
    ));
    transaction.SetDefaultAction(true);
  }
  exists = false;
  (void)db->TableExists(NS_LITERAL_CSTRING("test2"), &exists);
  do_check_true(exists);
}

void
test_null_database_connection()
{
  // We permit the use of the Transaction helper when passing a null database
  // in, so we need to make sure this still works without crashing.
  mozStorageTransaction transaction(nsnull, false);

  do_check_false(transaction.HasTransaction());
  do_check_true(NS_SUCCEEDED(transaction.Commit()));
  do_check_true(NS_SUCCEEDED(transaction.Rollback()));
}

void (*gTests[])(void) = {
  test_HasTransaction,
  test_Commit,
  test_Rollback,
  test_AutoCommit,
  test_AutoRollback,
  test_SetDefaultAction,
  test_null_database_connection,
};

const char *file = __FILE__;
#define TEST_NAME "transaction helper"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
