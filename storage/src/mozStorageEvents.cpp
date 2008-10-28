/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation. 
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsAutoLock.h"
#include "nsCOMArray.h"

#include "sqlite3.h"

#include "mozIStorageStatementCallback.h"
#include "mozIStoragePendingStatement.h"
#include "mozStorageHelper.h"
#include "mozStorageResultSet.h"
#include "mozStorageRow.h"
#include "mozStorageBackground.h"
#include "mozStorageError.h"
#include "mozStorageEvents.h"

////////////////////////////////////////////////////////////////////////////////
//// Asynchronous Statement Execution

/**
 * Enum used to describe the state of execution.
 */
enum ExecutionState {
    PENDING = -1
  , COMPLETED = mozIStorageStatementCallback::REASON_FINISHED
  , CANCELED = mozIStorageStatementCallback::REASON_CANCELED
  , ERROR = mozIStorageStatementCallback::REASON_ERROR
};

/**
 * Interface used to check if an event should run.
 */
class iEventStatus : public nsISupports
{
public:
  virtual PRBool runEvent() = 0;
};

/**
 * Notifies a callback with a result set.
 */
class CallbackResultNotifier : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  CallbackResultNotifier(mozIStorageStatementCallback *aCallback,
                         mozIStorageResultSet *aResults,
                         iEventStatus *aEventStatus) :
      mCallback(aCallback)
    , mResults(aResults)
    , mEventStatus(aEventStatus)
  {
  }

  NS_IMETHOD Run()
  {
    if (mEventStatus->runEvent())
      (void)mCallback->HandleResult(mResults);

    return NS_OK;
  }

private:
  CallbackResultNotifier() { }

  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<mozIStorageResultSet> mResults;
  nsRefPtr<iEventStatus> mEventStatus;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  CallbackResultNotifier,
  nsIRunnable
)

/**
 * Notifies the calling thread that an error has occurred.
 */
class ErrorNotifier : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  ErrorNotifier(mozIStorageStatementCallback *aCallback,
                mozIStorageError *aErrorObj,
                iEventStatus *aEventStatus) :
      mCallback(aCallback)
    , mErrorObj(aErrorObj)
    , mEventStatus(aEventStatus)
  {
  }

  NS_IMETHOD Run()
  {
    if (mEventStatus->runEvent() && mCallback)
      (void)mCallback->HandleError(mErrorObj);

    return NS_OK;
  }

private:
  ErrorNotifier() { }

  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<mozIStorageError> mErrorObj;
  nsRefPtr<iEventStatus> mEventStatus;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  ErrorNotifier,
  nsIRunnable
)

/**
 * Notifies the calling thread that the statement has finished executing.
 */
class CompletionNotifier : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  /**
   * This takes ownership of the callback.  It is released on the thread this is
   * dispatched to (which should always be the calling thread).
   */
  CompletionNotifier(mozIStorageStatementCallback *aCallback,
                     ExecutionState aReason) :
      mCallback(aCallback)
    , mReason(aReason)
  {
  }

  NS_IMETHOD Run()
  {
    (void)mCallback->HandleCompletion(mReason);
    NS_RELEASE(mCallback);

    return NS_OK;
  }

  virtual void cancel()
  {
    // Update our reason so the completion notifier knows what is up.
    mReason = CANCELED;
  }

private:
  CompletionNotifier() { }

  mozIStorageStatementCallback *mCallback;
  ExecutionState mReason;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  CompletionNotifier,
  nsIRunnable
)

/**
 * Executes a statement asynchronously in the background.
 */
class AsyncExecute : public nsIRunnable
                   , public mozIStoragePendingStatement
                   , public iEventStatus
{
public:
  NS_DECL_ISUPPORTS

  /**
   * This takes ownership of both the statement and the callback.
   */
  AsyncExecute(nsTArray<sqlite3_stmt *> &aStatements,
               mozIStorageConnection *aConnection,
               mozIStorageStatementCallback *aCallback) :
      mConnection(aConnection)
    , mTransactionManager(nsnull)
    , mCallback(aCallback)
    , mCallingThread(do_GetCurrentThread())
    , mState(PENDING)
    , mCancelRequested(PR_FALSE)
    , mLock(nsAutoLock::NewLock("AsyncExecute::mLock"))
  {
    (void)mStatements.SwapElements(aStatements);
    NS_ASSERTION(mStatements.Length(), "We weren't given any statements!");
  }

  nsresult initialize()
  {
    NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);
    NS_IF_ADDREF(mCallback);
    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    // do not run if we have been canceled
    {
      nsAutoLock mutex(mLock);
      if (mCancelRequested) {
        mState = CANCELED;
        mutex.unlock();
        return NotifyComplete();
      }
    }

    // If there is more than one statement, run it in a transaction.  We assume
    // that we have been given write statements since getting a batch of read
    // statements doesn't make a whole lot of sense.
    if (mStatements.Length() > 1) {
      // We don't error if this failed because it's not terrible if it does.
      mTransactionManager = new mozStorageTransaction(mConnection, PR_FALSE,
                                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);
    }

    // Execute each statement, giving the callback results if it returns any.
    nsresult rv = NS_OK;
    for (PRUint32 i = 0; i < mStatements.Length(); i++) {
      // We need to hold a lock for statement execution so we can properly
      // reflect state in case we are canceled.  We unlock in a few areas in
      // order to allow for cancelation to occur.
      nsAutoLock mutex(mLock);

      while (PR_TRUE) {
        int rc = sqlite3_step(mStatements[i]);
        // Break out if we have no more results
        if (rc == SQLITE_DONE)
          break;

        // Some errors are not fatal, and we can handle them and continue.
        if (rc != SQLITE_OK && rc != SQLITE_ROW) {
          if (rc == SQLITE_BUSY) {
            // We do not want to hold our lock while we yield.
            nsAutoUnlock cancelationScope(mLock);

            // Yield, and try again
            PR_Sleep(PR_INTERVAL_NO_WAIT);
            continue;
          }

          // Set error state
          mState = ERROR;

          // No longer need to hold our mutex
          mutex.unlock();

          // Notify
          sqlite3 *db = sqlite3_db_handle(mStatements[i]);
          (void)NotifyError(rc, sqlite3_errmsg(db));

          // And complete
          return NotifyComplete();
        }

        // If we do not have a callback, there's no point in executing this
        // statement anymore.
        if (!mCallback)
          break;

        // If we have been canceled, there is no point in going on...
        if (mCancelRequested) {
          mState = CANCELED;
          mutex.unlock();
          return NotifyComplete();
        }

        // For the rest of this loop, it is safe to not hold the lock and allow
        // for cancelation.  We may add an event to the calling thread, but that
        // thread will not end up running when it checks back with us to see if
        // it should run.
        nsAutoUnlock cancelationScope(mLock);

        // Build result object
        // XXX bug 454740 chunk these results better
        nsRefPtr<mozStorageResultSet> results(new mozStorageResultSet());
        if (!results) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }

        nsRefPtr<mozStorageRow> row(new mozStorageRow());
        if (!row) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }

        rv = row->initialize(mStatements[i]);
        if (NS_FAILED(rv))
          break;

        rv = results->add(row);
        if (NS_FAILED(rv))
          break;

        // Notify caller
        (void)NotifyResults(results);
      }

      // If we have an error that we have not already notified about, set our
      // state accordingly, and notify.
      if (NS_FAILED(rv)) {
        mState = ERROR;

        // We no longer need to hold our mutex
        mutex.unlock();
        (void)NotifyError(mozIStorageError::ERROR, "");
        break;
      }

      // If we are done, we need to set our state accordingly while we still
      // hold our lock.  We would have already dropped out of the loop if we
      // were canceled or had an error at this point.
      if (i == (mStatements.Length() - 1))
        mState = COMPLETED;
    }

    // Notify about completion
    return NotifyComplete();
  }

  NS_IMETHOD Cancel(PRBool *_successful)
  {
#ifdef DEBUG
    PRBool onCallingThread = PR_FALSE;
    (void)mCallingThread->IsOnCurrentThread(&onCallingThread);
    NS_ASSERTION(onCallingThread, "Not canceling from the calling thread!");
#endif

    // If we have already canceled, we have an error, but always indicate that
    // we are trying to cancel.
    NS_ENSURE_FALSE(mCancelRequested, NS_ERROR_UNEXPECTED);

    {
      nsAutoLock mutex(mLock);

      // We need to indicate that we want to try and cancel now.
      mCancelRequested = PR_TRUE;

      // Establish if we can cancel
      *_successful = (mState == PENDING);
    }

    // Note, it is possible for us to return false here, and end up canceling
    // events that have been dispatched to the calling thread.  This is OK,
    // however, because only read statements (such as SELECT) are going to be
    // posting events to the calling thread that actually check if they should
    // run or not.

    return NS_OK;
  }

  /**
   * This is part of iEventStatus.  It indicates if an event should be ran based
   * on if we are trying to cancel or not.
   */
  PRBool runEvent()
  {
#ifdef DEBUG
    PRBool onCallingThread = PR_FALSE;
    (void)mCallingThread->IsOnCurrentThread(&onCallingThread);
    NS_ASSERTION(onCallingThread, "runEvent not running on the calling thread!");
#endif

    // We do not need to acquire mLock here because it can only ever be written
    // to on the calling thread, and the only thread that can call us is the
    // calling thread, so we know that our access is serialized.
    return !mCancelRequested;
  }

private:
  AsyncExecute() { }

  ~AsyncExecute()
  {
    nsAutoLock::DestroyLock(mLock);
  }

  /**
   * Notifies callback about completion, and does any necessary cleanup.
   */
  nsresult NotifyComplete()
  {
    NS_ASSERTION(mState != PENDING,
                 "Still in a pending state when calling Complete!");

    // Handle our transaction, if we have one
    if (mTransactionManager) {
      if (mState == COMPLETED) {
        nsresult rv = mTransactionManager->Commit();
        if (NS_FAILED(rv)) {
          mState = ERROR;
          (void)NotifyError(mozIStorageError::ERROR,
                            "Transaction failed to commit");
        }
      }
      else {
        (void)mTransactionManager->Rollback();
      }
      delete mTransactionManager;
      mTransactionManager = nsnull;
    }

    // Finalize our statements
    for (PRUint32 i = 0; i < mStatements.Length(); i++) {
      (void)sqlite3_finalize(mStatements[i]);
      mStatements[i] = NULL;
    }

    // Notify about completion iff we have a callback.
    if (mCallback) {
      nsRefPtr<CompletionNotifier> completionEvent =
        new CompletionNotifier(mCallback, mState);
      NS_ENSURE_TRUE(completionEvent, NS_ERROR_OUT_OF_MEMORY);

      // We no longer own mCallback (the CompletionNotifier takes ownership).
      mCallback = nsnull;

      (void)mCallingThread->Dispatch(completionEvent, NS_DISPATCH_NORMAL);
    }

    return NS_OK;
  }

  /**
   * Notifies callback about an error.
   *
   * @param aErrorCode
   *        The error code defined in mozIStorageError for the error.
   * @param aMessage
   *        The error string, if any.
   */
  nsresult NotifyError(PRInt32 aErrorCode, const char *aMessage)
  {
    if (!mCallback)
      return NS_OK;

    nsCOMPtr<mozIStorageError> errorObj =
      new mozStorageError(aErrorCode, aMessage);
    NS_ENSURE_TRUE(errorObj, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<ErrorNotifier> notifier =
      new ErrorNotifier(mCallback, errorObj, this);
    NS_ENSURE_TRUE(notifier, NS_ERROR_OUT_OF_MEMORY);

    return mCallingThread->Dispatch(notifier, NS_DISPATCH_NORMAL);
  }

  /**
   * Notifies the callback about a result set.
   *
   * @param aResultSet
   *        The mozIStorageResultSet to notify the callback about.
   */
  nsresult NotifyResults(mozStorageResultSet *aResultSet)
  {
    NS_ASSERTION(mCallback, "NotifyResults called without a callback!");

    nsRefPtr<CallbackResultNotifier> notifier =
      new CallbackResultNotifier(mCallback, aResultSet, this);
    NS_ENSURE_TRUE(notifier, NS_ERROR_OUT_OF_MEMORY);

    return mCallingThread->Dispatch(notifier, NS_DISPATCH_NORMAL);
  };

  nsTArray<sqlite3_stmt *> mStatements;
  mozIStorageConnection *mConnection;
  mozStorageTransaction *mTransactionManager;
  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<nsIThread> mCallingThread;

  /**
   * Indicates the state the object is currently in.
   */
  ExecutionState mState;

  /**
   * Indicates if we should try to cancel at a cancelation point or not.
   */
  PRBool mCancelRequested;

  /**
   * This is the lock that protects our state from changing.  This includes the
   * following variables:
   *   -mState
   *   -mCancelRequested is only set on the calling thread while the lock is
   *    held.  It is always read from within the lock on the background thread,
   *    but not on the calling thread (see runEvent for why).
   */
  PRLock *mLock;
};
NS_IMPL_THREADSAFE_ISUPPORTS2(
  AsyncExecute,
  nsIRunnable,
  mozIStoragePendingStatement
)

nsresult
NS_executeAsync(nsTArray<sqlite3_stmt *> &aStatements,
                mozIStorageConnection *aConnection,
                mozIStorageStatementCallback *aCallback,
                mozIStoragePendingStatement **_stmt)
{
  // Create our event to run in the background
  nsRefPtr<AsyncExecute> event(new AsyncExecute(aStatements, aConnection, aCallback));
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = event->initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  // Dispatch it to the background
  nsIEventTarget *target = mozStorageBackground::getService()->target();
  rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return it as the pending statement object
  NS_ADDREF(*_stmt = event);
  return NS_OK;
}
