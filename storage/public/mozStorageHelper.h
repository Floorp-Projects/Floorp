/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Storage code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
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

#ifndef MOZSTORAGEHELPER_H
#define MOZSTORAGEHELPER_H

#include "nsAutoPtr.h"

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozStorage.h"


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
 */
class mozStorageTransaction
{
public:
  mozStorageTransaction(mozIStorageConnection* aConnection,
                        bool aCommitOnComplete,
                        PRInt32 aType = mozIStorageConnection::TRANSACTION_DEFERRED)
    : mConnection(aConnection),
      mHasTransaction(PR_FALSE),
      mCommitOnComplete(aCommitOnComplete),
      mCompleted(PR_FALSE)
  {
    // We won't try to get a transaction if one is already in progress.
    if (mConnection)
      mHasTransaction = NS_SUCCEEDED(mConnection->BeginTransactionAs(aType));
  }
  ~mozStorageTransaction()
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
    mCompleted = PR_TRUE;
    if (! mHasTransaction)
      return NS_OK; // transaction not ours, ignore
    nsresult rv = mConnection->CommitTransaction();
    if (NS_SUCCEEDED(rv))
      mHasTransaction = PR_FALSE;

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
    mCompleted = PR_TRUE;
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
      mHasTransaction = PR_FALSE;

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
  nsCOMPtr<mozIStorageConnection> mConnection;
  bool mHasTransaction;
  bool mCommitOnComplete;
  bool mCompleted;
};


/**
 * This class wraps a statement so that it is guaraneed to be reset when
 * this object goes out of scope.
 *
 * Note that this always just resets the statement. If the statement doesn't
 * need resetting, the reset operation is inexpensive.
 */
class NS_STACK_CLASS mozStorageStatementScoper
{
public:
  mozStorageStatementScoper(mozIStorageStatement* aStatement)
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
    mStatement = nsnull;
  }

protected:
  nsCOMPtr<mozIStorageStatement> mStatement;
};

#endif /* MOZSTORAGEHELPER_H */
