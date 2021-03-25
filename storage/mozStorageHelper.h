/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGEHELPER_H
#define MOZSTORAGEHELPER_H

#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"

#include "mozilla/storage/SQLiteMutex.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStoragePendingStatement.h"
#include "mozilla/DebugOnly.h"
#include "nsCOMPtr.h"
#include "nsError.h"

/**
 * This class wraps a transaction inside a given C++ scope, guaranteeing that
 * the transaction will be completed even if you have an exception or
 * return early.
 *
 * A common use is to create an instance with aCommitOnComplete = false
 * (rollback), then call Commit() on this object manually when your function
 * completes successfully.
 *
 * @note nested transactions are not supported by Sqlite, only nested
 * savepoints, so if a transaction is already in progress, this object creates
 * a nested savepoint to the existing transaction which is considered as
 * anonymous savepoint itself. However, aType and aAsyncCommit are ignored
 * in the case of nested savepoints.
 *
 * @param aConnection
 *        The connection to create the transaction on.
 * @param aCommitOnComplete
 *        Controls whether the transaction is committed or rolled back when
 *        this object goes out of scope.
 * @param aType [optional]
 *        The transaction type, as defined in mozIStorageConnection. Uses the
 *        default transaction behavior for the connection if unspecified.
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
class mozStorageTransaction {
  using SQLiteMutexAutoLock = mozilla::storage::SQLiteMutexAutoLock;

 public:
  mozStorageTransaction(
      mozIStorageConnection* aConnection, bool aCommitOnComplete,
      int32_t aType = mozIStorageConnection::TRANSACTION_DEFAULT,
      bool aAsyncCommit = false)
      : mConnection(aConnection),
        mType(aType),
        mNestingLevel(0),
        mHasTransaction(false),
        mCommitOnComplete(aCommitOnComplete),
        mCompleted(false),
        mAsyncCommit(aAsyncCommit) {}

  ~mozStorageTransaction() {
    if (mConnection && mHasTransaction && !mCompleted) {
      if (mCommitOnComplete) {
        mozilla::DebugOnly<nsresult> rv = Commit();
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "A transaction didn't commit correctly");
      } else {
        mozilla::DebugOnly<nsresult> rv = Rollback();
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "A transaction didn't rollback correctly");
      }
    }
  }

  /**
   * Starts the transaction.
   */
  nsresult Start() {
    // XXX We should probably get rid of mHasTransaction and use mConnection
    // for checking if a transaction has been started. However, we need to
    // first stop supporting null mConnection and also move aConnection from
    // the constructor to Start.
    MOZ_DIAGNOSTIC_ASSERT(!mHasTransaction);

    // XXX We should probably stop supporting null mConnection.

    // XXX We should probably get rid of mCompleted and allow to start the
    // transaction again if it was already committed or rolled back.
    if (!mConnection || mCompleted) {
      return NS_OK;
    }

    SQLiteMutexAutoLock lock(mConnection->GetSharedDBMutex());

    // We nee to speculatively set the nesting level to be able to decide
    // if this is a top level transaction and to be able to generate the
    // savepoint name.
    TransactionStarted(lock);

    // If there's a failure we need to revert the speculatively set nesting
    // level on the connection.
    auto autoFinishTransaction =
        mozilla::MakeScopeExit([&] { TransactionFinished(lock); });

    nsAutoCString query;

    if (TopLevelTransaction(lock)) {
      query.Assign("BEGIN");
      int32_t type = mType;
      if (type == mozIStorageConnection::TRANSACTION_DEFAULT) {
        MOZ_ALWAYS_SUCCEEDS(mConnection->GetDefaultTransactionType(&type));
      }
      switch (type) {
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
    } else {
      query.Assign("SAVEPOINT sp"_ns + IntToCString(mNestingLevel));
    }

    nsresult rv = mConnection->ExecuteSimpleSQL(query);
    NS_ENSURE_SUCCESS(rv, rv);

    autoFinishTransaction.release();

    return NS_OK;
  }

  /**
   * Commits the transaction if one is in progress. If one is not in progress,
   * this is a NOP since the actual owner of the transaction outside of our
   * scope is in charge of finally committing or rolling back the transaction.
   */
  nsresult Commit() {
    // XXX Assert instead of returning NS_OK if the transaction hasn't been
    // started.
    if (!mConnection || mCompleted || !mHasTransaction) return NS_OK;

    SQLiteMutexAutoLock lock(mConnection->GetSharedDBMutex());

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MOZ_DIAGNOSTIC_ASSERT(CurrentTransaction(lock));
#else
    if (!CurrentTransaction(lock)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
#endif

    mCompleted = true;

    nsresult rv;

    if (TopLevelTransaction(lock)) {
      // TODO (bug 559659): this might fail with SQLITE_BUSY, but we don't
      // handle it, thus the transaction might stay open until the next COMMIT.
      if (mAsyncCommit) {
        nsCOMPtr<mozIStoragePendingStatement> ps;
        rv = mConnection->ExecuteSimpleSQLAsync("COMMIT"_ns, nullptr,
                                                getter_AddRefs(ps));
      } else {
        rv = mConnection->ExecuteSimpleSQL("COMMIT"_ns);
      }
    } else {
      rv = mConnection->ExecuteSimpleSQL("RELEASE sp"_ns +
                                         IntToCString(mNestingLevel));
    }

    NS_ENSURE_SUCCESS(rv, rv);

    TransactionFinished(lock);

    return NS_OK;
  }

  /**
   * Rolls back the transaction if one is in progress. If one is not in
   * progress, this is a NOP since the actual owner of the transaction outside
   * of our scope is in charge of finally rolling back the transaction.
   */
  nsresult Rollback() {
    // XXX Assert instead of returning NS_OK if the transaction hasn't been
    // started.
    if (!mConnection || mCompleted || !mHasTransaction) return NS_OK;

    SQLiteMutexAutoLock lock(mConnection->GetSharedDBMutex());

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MOZ_DIAGNOSTIC_ASSERT(CurrentTransaction(lock));
#else
    if (!CurrentTransaction(lock)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
#endif

    mCompleted = true;

    nsresult rv;

    if (TopLevelTransaction(lock)) {
      // TODO (bug 1062823): from Sqlite 3.7.11 on, rollback won't ever return
      // a busy error, so this handling can be removed.
      do {
        rv = mConnection->ExecuteSimpleSQL("ROLLBACK"_ns);
        if (rv == NS_ERROR_STORAGE_BUSY) (void)PR_Sleep(PR_INTERVAL_NO_WAIT);
      } while (rv == NS_ERROR_STORAGE_BUSY);
    } else {
      const auto nestingLevelCString = IntToCString(mNestingLevel);
      rv = mConnection->ExecuteSimpleSQL(
          "ROLLBACK TO sp"_ns + nestingLevelCString + "; RELEASE sp"_ns +
          nestingLevelCString);
    }

    NS_ENSURE_SUCCESS(rv, rv);

    TransactionFinished(lock);

    return NS_OK;
  }

 protected:
  void TransactionStarted(const SQLiteMutexAutoLock& aProofOfLock) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(!mHasTransaction);
    MOZ_ASSERT(mNestingLevel == 0);
    mHasTransaction = true;
    mNestingLevel = mConnection->IncreaseTransactionNestingLevel(aProofOfLock);
  }

  bool CurrentTransaction(const SQLiteMutexAutoLock& aProofOfLock) const {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mHasTransaction);
    MOZ_ASSERT(mNestingLevel > 0);
    return mNestingLevel ==
           mConnection->GetTransactionNestingLevel(aProofOfLock);
  }

  bool TopLevelTransaction(const SQLiteMutexAutoLock& aProofOfLock) const {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mHasTransaction);
    MOZ_ASSERT(mNestingLevel > 0);
    MOZ_ASSERT(CurrentTransaction(aProofOfLock));
    return mNestingLevel == 1;
  }

  void TransactionFinished(const SQLiteMutexAutoLock& aProofOfLock) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mHasTransaction);
    MOZ_ASSERT(mNestingLevel > 0);
    MOZ_ASSERT(CurrentTransaction(aProofOfLock));
    mConnection->DecreaseTransactionNestingLevel(aProofOfLock);
    mNestingLevel = 0;
    mHasTransaction = false;
  }

  nsCOMPtr<mozIStorageConnection> mConnection;
  int32_t mType;
  uint32_t mNestingLevel;
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
class MOZ_STACK_CLASS mozStorageStatementScoper {
 public:
  explicit mozStorageStatementScoper(mozIStorageStatement* aStatement)
      : mStatement(aStatement) {}
  ~mozStorageStatementScoper() {
    if (mStatement) mStatement->Reset();
  }

  mozStorageStatementScoper(mozStorageStatementScoper&&) = default;
  mozStorageStatementScoper& operator=(mozStorageStatementScoper&&) = default;
  mozStorageStatementScoper(const mozStorageStatementScoper&) = delete;
  mozStorageStatementScoper& operator=(const mozStorageStatementScoper&) =
      delete;

  /**
   * Call this to make the statement not reset. You might do this if you know
   * that the statement has been reset.
   */
  void Abandon() { mStatement = nullptr; }

 protected:
  nsCOMPtr<mozIStorageStatement> mStatement;
};

// Use this to make queries uniquely identifiable in telemetry
// statistics, especially PRAGMAs.  We don't include __LINE__ so that
// queries are stable in the face of source code changes.
#define MOZ_STORAGE_UNIQUIFY_QUERY_STR "/* " __FILE__ " */ "

#endif /* MOZSTORAGEHELPER_H */
