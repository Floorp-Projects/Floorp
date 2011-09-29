/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set:sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
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
 * The Original Code is storage test code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
  DatabaseLocker(const char* aSQL)
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
    mConnection = nsnull;

    Notify(TEST_DONE);

    return NS_OK;
  }

private:
  nsCOMPtr<mozIStorageConnection> mConnection;
  State mState;
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
