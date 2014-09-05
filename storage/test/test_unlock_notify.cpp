/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set:sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozilla/ReentrantMonitor.h"
#include "nsThreadUtils.h"
#include "mozIStorageStatement.h"

/**
 * This file tests that our implementation around sqlite3_unlock_notify works
 * as expected.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helpers

enum State {
  STARTING,
  WRITE_LOCK,
  READ_LOCK,
  TEST_DONE
};

class DatabaseLocker : public nsRunnable
{
public:
  explicit DatabaseLocker(const char* aSQL)
  : monitor("DatabaseLocker::monitor")
  , mSQL(aSQL)
  , mState(STARTING)
  {
  }

  void RunInBackground()
  {
    (void)NS_NewThread(getter_AddRefs(mThread));
    do_check_true(mThread);

    do_check_success(mThread->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  NS_IMETHOD Run()
  {
    mozilla::ReentrantMonitorAutoEnter lock(monitor);

    nsCOMPtr<mozIStorageConnection> db(getDatabase());

    nsCString sql(mSQL);
    nsCOMPtr<mozIStorageStatement> stmt;
    do_check_success(db->CreateStatement(sql, getter_AddRefs(stmt)));

    bool hasResult;
    do_check_success(stmt->ExecuteStep(&hasResult));

    Notify(WRITE_LOCK);
    WaitFor(TEST_DONE);

    return NS_OK;
  }

  void WaitFor(State aState)
  {
    monitor.AssertCurrentThreadIn();
    while (mState != aState) {
      do_check_success(monitor.Wait());
    }
  }

  void Notify(State aState)
  {
    monitor.AssertCurrentThreadIn();
    mState = aState;
    do_check_success(monitor.Notify());
  }

  mozilla::ReentrantMonitor monitor;

protected:
  nsCOMPtr<nsIThread> mThread;
  const char *const mSQL;
  State mState;
};

class DatabaseTester : public DatabaseLocker
{
public:
  DatabaseTester(mozIStorageConnection *aConnection,
                 const char* aSQL)
  : DatabaseLocker(aSQL)
  , mConnection(aConnection)
  {
  }

  NS_IMETHOD Run()
  {
    mozilla::ReentrantMonitorAutoEnter lock(monitor);
    WaitFor(READ_LOCK);

    nsCString sql(mSQL);
    nsCOMPtr<mozIStorageStatement> stmt;
    do_check_success(mConnection->CreateStatement(sql, getter_AddRefs(stmt)));

    bool hasResult;
    nsresult rv = stmt->ExecuteStep(&hasResult);
    do_check_eq(rv, NS_ERROR_FILE_IS_LOCKED);

    // Finalize our statement and null out our connection before notifying to
    // ensure that we close on the proper thread.
    rv = stmt->Finalize();
    do_check_eq(rv, NS_ERROR_FILE_IS_LOCKED);
    mConnection = nullptr;

    Notify(TEST_DONE);

    return NS_OK;
  }

private:
  nsCOMPtr<mozIStorageConnection> mConnection;
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

void
setup()
{
  nsCOMPtr<mozIStorageConnection> db(getDatabase());

  // Create and populate a dummy table.
  nsresult rv = db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY, data STRING)"
  ));
  do_check_success(rv);
  rv = db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO test (data) VALUES ('foo')"
  ));
  do_check_success(rv);
  rv = db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO test (data) VALUES ('bar')"
  ));
  do_check_success(rv);
  rv = db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE UNIQUE INDEX unique_data ON test (data)"
  ));
  do_check_success(rv);
}

void
test_step_locked_does_not_block_main_thread()
{
  nsCOMPtr<mozIStorageConnection> db(getDatabase());

  // Need to prepare our statement ahead of time so we make sure to only test
  // step and not prepare.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = db->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO test (data) VALUES ('test1')"
  ), getter_AddRefs(stmt));
  do_check_success(rv);

  nsRefPtr<DatabaseLocker> locker(new DatabaseLocker("SELECT * FROM test"));
  do_check_true(locker);
  mozilla::ReentrantMonitorAutoEnter lock(locker->monitor);
  locker->RunInBackground();

  // Wait for the locker to notify us that it has locked the database properly.
  locker->WaitFor(WRITE_LOCK);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  do_check_eq(rv, NS_ERROR_FILE_IS_LOCKED);

  locker->Notify(TEST_DONE);
}

void
test_drop_index_does_not_loop()
{
  nsCOMPtr<mozIStorageConnection> db(getDatabase());

  // Need to prepare our statement ahead of time so we make sure to only test
  // step and not prepare.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM test"
  ), getter_AddRefs(stmt));
  do_check_success(rv);

  nsRefPtr<DatabaseTester> tester =
    new DatabaseTester(db, "DROP INDEX unique_data");
  do_check_true(tester);
  mozilla::ReentrantMonitorAutoEnter lock(tester->monitor);
  tester->RunInBackground();

  // Hold a read lock on the database, and then let the tester try to execute.
  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  do_check_success(rv);
  do_check_true(hasResult);
  tester->Notify(READ_LOCK);

  // Make sure the tester finishes its test before we move on.
  tester->WaitFor(TEST_DONE);
}

void
test_drop_table_does_not_loop()
{
  nsCOMPtr<mozIStorageConnection> db(getDatabase());

  // Need to prepare our statement ahead of time so we make sure to only test
  // step and not prepare.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM test"
  ), getter_AddRefs(stmt));
  do_check_success(rv);

  nsRefPtr<DatabaseTester> tester(new DatabaseTester(db, "DROP TABLE test"));
  do_check_true(tester);
  mozilla::ReentrantMonitorAutoEnter lock(tester->monitor);
  tester->RunInBackground();

  // Hold a read lock on the database, and then let the tester try to execute.
  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  do_check_success(rv);
  do_check_true(hasResult);
  tester->Notify(READ_LOCK);

  // Make sure the tester finishes its test before we move on.
  tester->WaitFor(TEST_DONE);
}

void (*gTests[])(void) = {
  setup,
  test_step_locked_does_not_block_main_thread,
  test_drop_index_does_not_loop,
  test_drop_table_does_not_loop,
};

const char *file = __FILE__;
#define TEST_NAME "sqlite3_unlock_notify"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
