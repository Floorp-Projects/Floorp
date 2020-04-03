/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_StatementCache_h
#define mozilla_storage_StatementCache_h

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"

#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"

namespace mozilla {
namespace storage {

/**
 * Class used to cache statements (mozIStorageStatement or
 * mozIStorageAsyncStatement).
 */
template <typename StatementType>
class StatementCache {
 public:
  /**
   * Constructor for the cache.
   *
   * @note a connection can have more than one cache.
   *
   * @param aConnection
   *        A reference to the nsCOMPtr for the connection this cache is to be
   *        used for.  This nsCOMPtr must at least live as long as this class,
   *        otherwise crashes will happen.
   */
  explicit StatementCache(nsCOMPtr<mozIStorageConnection>& aConnection)
      : mConnection(aConnection) {}

  /**
   * Obtains a cached statement.  If this statement is not yet created, it will
   * be created and stored for later use.
   *
   * @param aQuery
   *        The SQL string (either a const char [] or nsACString) to get a
   *        cached query for.
   * @return the cached statement, or null upon error.
   */
  inline already_AddRefed<StatementType> GetCachedStatement(
      const nsACString& aQuery) {
    nsCOMPtr<StatementType> stmt;
    if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
      stmt = CreateStatement(aQuery);
      NS_ENSURE_TRUE(stmt, nullptr);

      mCachedStatements.Put(aQuery, stmt);
    }
    return stmt.forget();
  }

  template <int N>
  MOZ_ALWAYS_INLINE already_AddRefed<StatementType> GetCachedStatement(
      const char (&aQuery)[N]) {
    nsDependentCString query(aQuery, N - 1);
    return GetCachedStatement(query);
  }

  /**
   * Finalizes all cached statements so the database can be safely closed.  The
   * behavior of this cache is unspecified after this method is called.
   */
  inline void FinalizeStatements() {
    for (auto iter = mCachedStatements.Iter(); !iter.Done(); iter.Next()) {
      (void)iter.Data()->Finalize();
    }

    // Clear the cache at this time too!
    (void)mCachedStatements.Clear();
  }

 private:
  inline already_AddRefed<StatementType> CreateStatement(
      const nsACString& aQuery);

  nsInterfaceHashtable<nsCStringHashKey, StatementType> mCachedStatements;
  nsCOMPtr<mozIStorageConnection>& mConnection;
};

template <>
inline already_AddRefed<mozIStorageStatement>
StatementCache<mozIStorageStatement>::CreateStatement(
    const nsACString& aQuery) {
  NS_ENSURE_TRUE(mConnection, nullptr);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    nsCString error;
    error.AppendLiteral("The statement '");
    error.Append(aQuery);
    error.AppendLiteral("' failed to compile with the error message '");
    nsCString msg;
    (void)mConnection->GetLastErrorString(msg);
    error.Append(msg);
    error.AppendLiteral("'.");
    NS_ERROR(error.get());
  }
  NS_ENSURE_SUCCESS(rv, nullptr);

  return stmt.forget();
}

template <>
inline already_AddRefed<mozIStorageAsyncStatement>
StatementCache<mozIStorageAsyncStatement>::CreateStatement(
    const nsACString& aQuery) {
  NS_ENSURE_TRUE(mConnection, nullptr);

  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  nsresult rv = mConnection->CreateAsyncStatement(aQuery, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return stmt.forget();
}

}  // namespace storage
}  // namespace mozilla

#endif  // mozilla_storage_StatementCache_h
