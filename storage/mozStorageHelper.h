/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGEHELPER_H
#define MOZSTORAGEHELPER_H

#include "nsAutoPtr.h"
#include "nsStringGlue.h"
#include "mozilla/DebugOnly.h"

#include "mozIStorageAsyncConnection.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStoragePendingStatement.h"
#include "nsError.h"

/**
 * This class wraps a transaction inside a given C++ scope, guaranteeing that
 * the transaction will be completed even if you have an exception or
 * return early.
 *
 * A common use is to create an instance with aCommitOnComplete = false (rollback),
 * then call Commit() on this object manually when your function completes
 * successfully.
 *
 * @note nested transactions are not supported by Sqlite, so if a transaction
 * is already in progress, this object does nothing.  Note that in this case,
 * you may not get the transaction type you asked for, and you won't be able
 * to rollback.
 *
 * @param aConnection
 *        The connection to create the transaction on.
 * @param aCommitOnComplete
 *        Controls whether the transaction is committed or rolled back when
 *        this object goes out of scope.
 * @param aType [optional]
 *        The transaction type, as defined in mozIStorageConnection.  Defaults
 *        to TRANSACTION_DEFERRED.
 * @param aAsyncCommit [optional]
 *        Whether commit should be executed asynchronously on the helper thread.
 *        This is a special option introduced as an interim solution to reduce
 *        main-thread fsyncs in Places.  Can only be used on main-thread.
 *
 *        WARNING: YOU SHOULD _NOT_ WRITE NEW MAIN-THREAD CODE USING THIS!
 *
 *        Notice that async commit might cause synchronous statements to fail
 *        with SQLITE_BUSY.  A possible mitigation strategy is to use
 *        PRAGMA busy_timeout, but notice that might cause main-thread jank.
 *        Finally, if the database is using WAL journaling mode, other
 *        connections won't see the changes done in async committed transactions
 *        until commit is complete.
 *
 *        For all of the above reasons, this should only be used as an interim
 *        solution and avoided completely if possible.
 */
class mozStorageTransaction
{
public:
  mozStorageTransaction(mozIStorageConnection* aConnection,
                        bool aCommitOnComplete,
                        int32_t aType = mozIStorageConnection::TRANSACTION_DEFERRED,
                        bool aAsyncCommit = false)
    : mConnection(aConnection),
      mHasTransaction(false),
      mCommitOnComplete(aCommitOnComplete),
      mCompleted(false),
      mAsyncCommit(aAsyncCommit)
  {
    if (mConnection) {
      nsAutoCString query("BEGIN");
      switch(aType) {
        case mozIStorageConnection::TRANSACTION_IMMEDIATE:
          query.AppendLiteral(" IMMEDIATE");
          break;
        case mozIStorageConnection::TRANSACTION_EXCLUSIVE:
          query.AppendLiteral(" EXCLUSIVE");
          break;
        case mozIStorageConnection::TRANSACTION_DEFERRED:
          query.AppendLiteral(" DEFERRED");
          break;
        default:
          MOZ_ASSERT(false, "Unknown transaction type");
      }
      // If a transaction is already in progress, this will fail, since Sqlite
      // doesn't support nested transactions.
      mHasTransaction = NS_SUCCEEDED(mConnection->ExecuteSimpleSQL(query));
    }
  }

  ~mozStorageTransaction()
  {
    if (mConnection && mHasTransaction && !mCompleted) {
      if (mCommitOnComplete) {
        mozilla::DebugOnly<nsresult> rv = Commit();
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                         "A transaction didn't commit correctly");
      }
      else {
        mozilla::DebugOnly<nsresult> rv = Rollback();
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                         "A transaction didn't rollback correctly");
      }
    }
  }

  /**
   * Commits the transaction if one is in progress. If one is not in progress,
   * this is a NOP since the actual owner of the transaction outside of our
   * scope is in charge of finally committing or rolling back the transaction.
   */
  nsresult Commit()
  {
    if (!mConnection || mCompleted || !mHasTransaction)
      return NS_OK;
    mCompleted = true;

    // TODO (bug 559659): this might fail with SQLITE_BUSY, but we don't handle
    // it, thus the transaction might stay open until the next COMMIT.
    nsresult rv;
    if (mAsyncCommit) {
      nsCOMPtr<mozIStoragePendingStatement> ps;
      rv = mConnection->ExecuteSimpleSQLAsync(NS_LITERAL_CSTRING("COMMIT"),
                                              nullptr, getter_AddRefs(ps));
    }
    else {
      rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("COMMIT"));
    }

    if (NS_SUCCEEDED(rv))
      mHasTransaction = false;

    return rv;
  }

  /**
   * Rolls back the transaction if one is in progress. If one is not in progress,
   * this is a NOP since the actual owner of the transaction outside of our
   * scope is in charge of finally rolling back the transaction.
   */
  nsresult Rollback()
  {
    if (!mConnection || mCompleted || !mHasTransaction)
      return NS_OK;
    mCompleted = true;

    // TODO (bug 1062823): from Sqlite 3.7.11 on, rollback won't ever return
    // a busy error, so this handling can be removed.
    nsresult rv = NS_OK;
    do {
      rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING("ROLLBACK"));
      if (rv == NS_ERROR_STORAGE_BUSY)
        (void)PR_Sleep(PR_INTERVAL_NO_WAIT);
    } while (rv == NS_ERROR_STORAGE_BUSY);

    if (NS_SUCCEEDED(rv))
      mHasTransaction = false;

    return rv;
  }

protected:
  nsCOMPtr<mozIStorageConnection> mConnection;
  bool mHasTransaction;
  bool mCommitOnComplete;
  bool mCompleted;
  bool mAsyncCommit;
};

/**
 * This class wraps a statement so that it is guaraneed to be reset when
 * this object goes out of scope.
 *
 * Note that this always just resets the statement. If the statement doesn't
 * need resetting, the reset operation is inexpensive.
 */
class MOZ_STACK_CLASS mozStorageStatementScoper
{
public:
  explicit mozStorageStatementScoper(mozIStorageStatement* aStatement)
      : mStatement(aStatement)
  {
  }
  ~mozStorageStatementScoper()
  {
    if (mStatement)
      mStatement->Reset();
  }

  /**
   * Call this to make the statement not reset. You might do this if you know
   * that the statement has been reset.
   */
  void Abandon()
  {
    mStatement = nullptr;
  }

protected:
  nsCOMPtr<mozIStorageStatement> mStatement;
};

// Use this to make queries uniquely identifiable in telemetry
// statistics, especially PRAGMAs.  We don't include __LINE__ so that
// queries are stable in the face of source code changes.
#define MOZ_STORAGE_UNIQUIFY_QUERY_STR "/* " __FILE__ " */ "

#endif /* MOZSTORAGEHELPER_H */
