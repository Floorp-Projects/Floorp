/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGEHELPER_H
#define MOZSTORAGEHELPER_H

#include "nsAutoPtr.h"

#include "mozIStorageAsyncConnection.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsError.h"

/**
 * This class wraps a transaction inside a given C++ scope, guaranteeing that
 * the transaction will be completed even if you have an exception or
 * return early.
 *
 * aCommitOnComplete controls whether the transaction is committed or rolled
 * back when it goes out of scope. A common use is to create an instance with
 * commitOnComplete = FALSE (rollback), then call Commit on this object manually
 * when your function completes successfully.
 *
 * Note that nested transactions are not supported by sqlite, so if a transaction
 * is already in progress, this object does nothing. Note that in this case,
 * you may not get the transaction type you ask for, and you won't be able
 * to rollback.
 *
 * Note: This class is templatized to be also usable with internal data
 * structures. External users of this class should generally use
 * |mozStorageTransaction| instead.
 */
template<typename T, typename U>
class mozStorageTransactionBase
{
public:
  mozStorageTransactionBase(T* aConnection,
                            bool aCommitOnComplete,
                            int32_t aType = mozIStorageConnection::TRANSACTION_DEFERRED)
    : mConnection(aConnection),
      mHasTransaction(false),
      mCommitOnComplete(aCommitOnComplete),
      mCompleted(false)
  {
    // We won't try to get a transaction if one is already in progress.
    if (mConnection)
      mHasTransaction = NS_SUCCEEDED(mConnection->BeginTransactionAs(aType));
  }
  ~mozStorageTransactionBase()
  {
    if (mConnection && mHasTransaction && ! mCompleted) {
      if (mCommitOnComplete)
        mConnection->CommitTransaction();
      else
        mConnection->RollbackTransaction();
    }
  }

  /**
   * Commits the transaction if one is in progress. If one is not in progress,
   * this is a NOP since the actual owner of the transaction outside of our
   * scope is in charge of finally comitting or rolling back the transaction.
   */
  nsresult Commit()
  {
    if (!mConnection || mCompleted)
      return NS_OK; // no connection, or already done
    mCompleted = true;
    if (! mHasTransaction)
      return NS_OK; // transaction not ours, ignore
    nsresult rv = mConnection->CommitTransaction();
    if (NS_SUCCEEDED(rv))
      mHasTransaction = false;

    return rv;
  }

  /**
   * Rolls back the transaction in progress. You should only call this function
   * if this object has a real transaction (HasTransaction() = true) because
   * otherwise, there is no transaction to roll back.
   */
  nsresult Rollback()
  {
    if (!mConnection || mCompleted)
      return NS_OK; // no connection, or already done
    mCompleted = true;
    if (! mHasTransaction)
      return NS_ERROR_FAILURE;

    // It is possible that a rollback will return busy, so we busy wait...
    nsresult rv = NS_OK;
    do {
      rv = mConnection->RollbackTransaction();
      if (rv == NS_ERROR_STORAGE_BUSY)
        (void)PR_Sleep(PR_INTERVAL_NO_WAIT);
    } while (rv == NS_ERROR_STORAGE_BUSY);

    if (NS_SUCCEEDED(rv))
      mHasTransaction = false;

    return rv;
  }

  /**
   * Returns whether this object wraps a real transaction. False means that
   * this object doesn't do anything because there was already a transaction in
   * progress when it was created.
   */
  bool HasTransaction()
  {
    return mHasTransaction;
  }

  /**
   * This sets the default action (commit or rollback) when this object goes
   * out of scope.
   */
  void SetDefaultAction(bool aCommitOnComplete)
  {
    mCommitOnComplete = aCommitOnComplete;
  }

protected:
  U mConnection;
  bool mHasTransaction;
  bool mCommitOnComplete;
  bool mCompleted;
};

/**
 * An instance of the mozStorageTransaction<> family dedicated
 * to |mozIStorageConnection|.
 */
typedef mozStorageTransactionBase<mozIStorageConnection,
                                  nsCOMPtr<mozIStorageConnection> >
mozStorageTransaction;



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
