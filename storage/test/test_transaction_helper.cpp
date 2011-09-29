/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
    mozStorageTransaction transaction(db, PR_FALSE);
    do_check_true(transaction.HasTransaction());
    (void)transaction.Commit();
    // And that it does not have a transaction after we have committed.
    do_check_false(transaction.HasTransaction());
  }

  // Check that no transaction is had after a rollback.
  {
    mozStorageTransaction transaction(db, PR_FALSE);
    do_check_true(transaction.HasTransaction());
    (void)transaction.Rollback();
    do_check_false(transaction.HasTransaction());
  }

  // Check that we do not have a transaction if one is already obtained.
  mozStorageTransaction outerTransaction(db, PR_FALSE);
  do_check_true(outerTransaction.HasTransaction());
  {
    mozStorageTransaction innerTransaction(db, PR_FALSE);
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
    mozStorageTransaction transaction(db, PR_FALSE);
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
    mozStorageTransaction transaction(db, PR_TRUE);
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
    mozStorageTransaction transaction(db, PR_TRUE);
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
    mozStorageTransaction transaction(db, PR_FALSE);
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
    mozStorageTransaction transaction(db, PR_TRUE);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test1 (id INTEGER PRIMARY KEY)"
    ));
    transaction.SetDefaultAction(PR_FALSE);
  }
  bool exists = true;
  (void)db->TableExists(NS_LITERAL_CSTRING("test1"), &exists);
  do_check_false(exists);

  // Now we do the opposite and test that a commit happens when we first set it
  // to automatically rollback.
  {
    mozStorageTransaction transaction(db, PR_FALSE);
    (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE test2 (id INTEGER PRIMARY KEY)"
    ));
    transaction.SetDefaultAction(PR_TRUE);
  }
  exists = PR_FALSE;
  (void)db->TableExists(NS_LITERAL_CSTRING("test2"), &exists);
  do_check_true(exists);
}

void
test_null_database_connection()
{
  // We permit the use of the Transaction helper when passing a null database
  // in, so we need to make sure this still works without crashing.
  mozStorageTransaction transaction(nsnull, PR_FALSE);

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
