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
 * The Original Code is the mozStorage statement cache.
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

#ifndef mozilla_storage_StatementCache_h
#define mozilla_storage_StatementCache_h

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"

#include "nsAutoPtr.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"

namespace mozilla {
namespace storage {

/**
 * Class used to cache statements (mozIStorageStatement or
 * mozIStorageAsyncStatement).
 */
template<typename StatementType>
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
  StatementCache(nsCOMPtr<mozIStorageConnection>& aConnection)
  : mConnection(aConnection)
  {
    if (!mCachedStatements.Init()) {
      NS_ERROR("Out of memory!?");
    }
  }

  /**
   * Obtains a cached statement.  If this statement is not yet created, it will
   * be created and stored for later use.
   *
   * @param aQuery
   *        The SQL string (either a const char [] or nsACString) to get a
   *        cached query for.
   * @return the cached statement, or null upon error.
   */
  inline
  already_AddRefed<StatementType>
  GetCachedStatement(const nsACString& aQuery)
  {
    nsCOMPtr<StatementType> stmt;
    if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
      stmt = CreateStatement(aQuery);
      NS_ENSURE_TRUE(stmt, nsnull);

      if (!mCachedStatements.Put(aQuery, stmt)) {
        NS_ERROR("Out of memory!?");
      }
    }
    return stmt.forget();
  }

  template<int N>
  NS_ALWAYS_INLINE already_AddRefed<StatementType>
  GetCachedStatement(const char (&aQuery)[N])
  {
    nsDependentCString query(aQuery, N - 1);
    return GetCachedStatement(query);
  }

  /**
   * Finalizes all cached statements so the database can be safely closed.  The
   * behavior of this cache is unspecified after this method is called.
   */
  inline
  void
  FinalizeStatements()
  {
    (void)mCachedStatements.Enumerate(FinalizeCachedStatements, NULL);

    // Clear the cache at this time too!
    (void)mCachedStatements.Clear();
  }

private:
  inline
  already_AddRefed<StatementType>
  CreateStatement(const nsACString& aQuery);
  static
  PLDHashOperator
  FinalizeCachedStatements(const nsACString& aKey,
                           nsCOMPtr<StatementType>& aStatement,
                           void*)
  {
    (void)aStatement->Finalize();
    return PL_DHASH_NEXT;
  }

  nsInterfaceHashtable<nsCStringHashKey, StatementType> mCachedStatements;
  nsCOMPtr<mozIStorageConnection>& mConnection;
};

template< >
inline
already_AddRefed<mozIStorageStatement>
StatementCache<mozIStorageStatement>::CreateStatement(const nsACString& aQuery)
{
  NS_ENSURE_TRUE(mConnection, nsnull);

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
  NS_ENSURE_SUCCESS(rv, nsnull);

  return stmt.forget();
}

template< >
inline
already_AddRefed<mozIStorageAsyncStatement>
StatementCache<mozIStorageAsyncStatement>::CreateStatement(const nsACString& aQuery)
{
  NS_ENSURE_TRUE(mConnection, nsnull);

  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  nsresult rv = mConnection->CreateAsyncStatement(aQuery, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return stmt.forget();
}

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_StatementCache_h
