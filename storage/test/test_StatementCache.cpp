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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "mozilla/storage/StatementCache.h"
using namespace mozilla::storage;

/**
 * This file test our statement cache in StatementCache.h.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helpers

class SyncCache : public StatementCache<mozIStorageStatement>
{
public:
  SyncCache(nsCOMPtr<mozIStorageConnection>& aConnection)
  : StatementCache<mozIStorageStatement>(aConnection)
  {
  }
};

class AsyncCache : public StatementCache<mozIStorageAsyncStatement>
{
public:
  AsyncCache(nsCOMPtr<mozIStorageConnection>& aConnection)
  : StatementCache<mozIStorageAsyncStatement>(aConnection)
  {
  }
};

/**
 * Wraps nsCString so we can not implement the same functions twice for each
 * type.
 */
class StringWrapper : public nsCString
{
public:
  StringWrapper(const char* aOther)
  {
    this->Assign(aOther);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

template<typename StringType>
void
test_GetCachedStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  SyncCache cache(db);

  StringType sql = "SELECT * FROM sqlite_master";

  // Make sure we get a statement back with the right state.
  nsCOMPtr<mozIStorageStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);
  PRInt32 state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_READY, state);

  // Check to make sure we get the same copy the second time we ask.
  nsCOMPtr<mozIStorageStatement> stmt2 = cache.GetCachedStatement(sql);
  do_check_true(stmt2);
  do_check_eq(stmt.get(), stmt2.get());
}

template <typename StringType>
void
test_FinalizeStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  SyncCache cache(db);

  StringType sql = "SELECT * FROM sqlite_master";

  // Get a statement, and then tell the cache to finalize.
  nsCOMPtr<mozIStorageStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);

  cache.FinalizeStatements();

  // We should be in an invalid state at this point.
  PRInt32 state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_INVALID, state);

  // Should be able to close the database now too.
  do_check_success(db->Close());
}

template<typename StringType>
void
test_GetCachedAsyncStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  AsyncCache cache(db);

  StringType sql = "SELECT * FROM sqlite_master";

  // Make sure we get a statement back with the right state.
  nsCOMPtr<mozIStorageAsyncStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);
  PRInt32 state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_READY, state);

  // Check to make sure we get the same copy the second time we ask.
  nsCOMPtr<mozIStorageAsyncStatement> stmt2 = cache.GetCachedStatement(sql);
  do_check_true(stmt2);
  do_check_eq(stmt.get(), stmt2.get());
}

template <typename StringType>
void
test_FinalizeAsyncStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  AsyncCache cache(db);

  StringType sql = "SELECT * FROM sqlite_master";

  // Get a statement, and then tell the cache to finalize.
  nsCOMPtr<mozIStorageAsyncStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);

  cache.FinalizeStatements();

  // We should be in an invalid state at this point.
  PRInt32 state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_INVALID, state);

  // Should be able to close the database now too.
  do_check_success(db->AsyncClose(nsnull));
}

////////////////////////////////////////////////////////////////////////////////
//// Test Harness Stuff

void (*gTests[])(void) = {
  test_GetCachedStatement<const char []>,
  test_GetCachedStatement<StringWrapper>,
  test_FinalizeStatements<const char []>,
  test_FinalizeStatements<StringWrapper>,
  test_GetCachedAsyncStatement<const char []>,
  test_GetCachedAsyncStatement<StringWrapper>,
  test_FinalizeAsyncStatements<const char []>,
  test_FinalizeAsyncStatements<StringWrapper>,
};

const char *file = __FILE__;
#define TEST_NAME "StatementCache"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
