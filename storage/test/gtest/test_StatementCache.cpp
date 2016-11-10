/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozilla/Attributes.h"
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
  explicit SyncCache(nsCOMPtr<mozIStorageConnection>& aConnection)
  : StatementCache<mozIStorageStatement>(aConnection)
  {
  }
};

class AsyncCache : public StatementCache<mozIStorageAsyncStatement>
{
public:
  explicit AsyncCache(nsCOMPtr<mozIStorageConnection>& aConnection)
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
  MOZ_IMPLICIT StringWrapper(const char* aOther)
  {
    this->Assign(aOther);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

// This is some gtest magic that allows us to parameterize tests by |const
// char[]| and |StringWrapper|.
template <typename T>
class storage_StatementCache : public ::testing::Test {};
typedef ::testing::Types<const char[], StringWrapper> TwoStringTypes;

TYPED_TEST_CASE(storage_StatementCache, TwoStringTypes);
TYPED_TEST(storage_StatementCache, GetCachedStatement)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  SyncCache cache(db);

  TypeParam sql = "SELECT * FROM sqlite_master";

  // Make sure we get a statement back with the right state.
  nsCOMPtr<mozIStorageStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);
  int32_t state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_READY, state);

  // Check to make sure we get the same copy the second time we ask.
  nsCOMPtr<mozIStorageStatement> stmt2 = cache.GetCachedStatement(sql);
  do_check_true(stmt2);
  do_check_eq(stmt.get(), stmt2.get());
}

TYPED_TEST_CASE(storage_StatementCache, TwoStringTypes);
TYPED_TEST(storage_StatementCache, FinalizeStatements)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  SyncCache cache(db);

  TypeParam sql = "SELECT * FROM sqlite_master";

  // Get a statement, and then tell the cache to finalize.
  nsCOMPtr<mozIStorageStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);

  cache.FinalizeStatements();

  // We should be in an invalid state at this point.
  int32_t state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_INVALID, state);

  // Should be able to close the database now too.
  do_check_success(db->Close());
}

TYPED_TEST_CASE(storage_StatementCache, TwoStringTypes);
TYPED_TEST(storage_StatementCache, GetCachedAsyncStatement)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  AsyncCache cache(db);

  TypeParam sql = "SELECT * FROM sqlite_master";

  // Make sure we get a statement back with the right state.
  nsCOMPtr<mozIStorageAsyncStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);
  int32_t state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_READY, state);

  // Check to make sure we get the same copy the second time we ask.
  nsCOMPtr<mozIStorageAsyncStatement> stmt2 = cache.GetCachedStatement(sql);
  do_check_true(stmt2);
  do_check_eq(stmt.get(), stmt2.get());
}

TYPED_TEST_CASE(storage_StatementCache, TwoStringTypes);
TYPED_TEST(storage_StatementCache, FinalizeAsyncStatements)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());
  AsyncCache cache(db);

  TypeParam sql = "SELECT * FROM sqlite_master";

  // Get a statement, and then tell the cache to finalize.
  nsCOMPtr<mozIStorageAsyncStatement> stmt = cache.GetCachedStatement(sql);
  do_check_true(stmt);

  cache.FinalizeStatements();

  // We should be in an invalid state at this point.
  int32_t state;
  do_check_success(stmt->GetState(&state));
  do_check_eq(mozIStorageBaseStatement::MOZ_STORAGE_STATEMENT_INVALID, state);

  // Should be able to close the database now too.
  do_check_success(db->AsyncClose(nullptr));
}

