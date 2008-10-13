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
 * Interface used to cancel pending events.
 */
class iCancelable : public nsISupports
{
public:
  /**
   * Tells an event to cancel itself.
   */
  virtual void cancel() = 0;
};

/**
 * Interface used to notify of event completion.
 */
class iCompletionNotifier : public nsISupports
{
public:
  /**
   * Called when an event is completed and no longer needs to be tracked.
   *
   * @param aEvent
   *        The event that has finished.
   */
  virtual void completed(iCancelable *aEvent) = 0;
};

/**
 * Notifies a callback with a result set.
 */
class CallbackResultNotifier : public nsIRunnable
                             , public iCancelable
{
public:
  NS_DECL_ISUPPORTS

  CallbackResultNotifier(mozIStorageStatementCallback *aCallback,
                         mozIStorageResultSet *aResults,
                         iCompletionNotifier *aNotifier) :
      mCallback(aCallback)
    , mResults(aResults)
    , mCompletionNotifier(aNotifier)
    , mCanceled(PR_FALSE)
  {
  }

  NS_IMETHOD Run()
  {
    if (!mCanceled)
      (void)mCallback->HandleResult(mResults);

    // Notify owner AsyncExecute that we have completed
    mCompletionNotifier->completed(this);
    // It is likely that the completion notifier holds a reference to us as
    // well, so we release our reference to it here to avoid cycles.
    mCompletionNotifier = nsnull;
    return NS_OK;
  }

  virtual void cancel()
  {
    // Atomically set our status so we know to not run.
    PR_AtomicSet(&mCanceled, PR_TRUE);
  }
private:
  CallbackResultNotifier() { }

  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<mozIStorageResultSet> mResults;
  nsRefPtr<iCompletionNotifier> mCompletionNotifier;
  PRInt32 mCanceled;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  CallbackResultNotifier,
  nsIRunnable
)

/**
 * Notifies the calling thread that an error has occurred.
 */
class ErrorNotifier : public nsIRunnable
                    , public iCancelable
{
public:
  NS_DECL_ISUPPORTS

  ErrorNotifier(mozIStorageStatementCallback *aCallback,
                mozIStorageError *aErrorObj,
                iCompletionNotifier *aCompletionNotifier) :
      mCallback(aCallback)
    , mErrorObj(aErrorObj)
    , mCanceled(PR_FALSE)
    , mCompletionNotifier(aCompletionNotifier)
  {
  }

  NS_IMETHOD Run()
  {
    if (!mCanceled && mCallback)
      (void)mCallback->HandleError(mErrorObj);

    mCompletionNotifier->completed(this);
    // It is likely that the completion notifier holds a reference to us as
    // well, so we release our reference to it here to avoid cycles.
    mCompletionNotifier = nsnull;
    return NS_OK;
  }

  virtual void cancel()
  {
    // Atomically set our status so we know to not run.
    PR_AtomicSet(&mCanceled, PR_TRUE);
  }

  static inline iCancelable *Dispatch(nsIThread *aCallingThread,
                                      mozIStorageStatementCallback *aCallback,
                                      iCompletionNotifier *aCompletionNotifier,
                                      int aResult,
                                      const char *aMessage)
  {
    nsCOMPtr<mozIStorageError> errorObj(new mozStorageError(aResult, aMessage));
    if (!errorObj)
      return nsnull;

    ErrorNotifier *notifier =
      new ErrorNotifier(aCallback, errorObj, aCompletionNotifier);
    (void)aCallingThread->Dispatch(notifier, NS_DISPATCH_NORMAL);
    return notifier;
  }
private:
  ErrorNotifier() { }

  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<mozIStorageError> mErrorObj;
  PRInt32 mCanceled;
  nsRefPtr<iCompletionNotifier> mCompletionNotifier;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(
  ErrorNotifier,
  nsIRunnable
)

/**
 * Notifies the calling thread that the statement has finished executing.
 */
class CompletionNotifier : public nsIRunnable
                         , public iCancelable
{
public:
  NS_DECL_ISUPPORTS

  /**
   * This takes ownership of the callback.  It is released on the thread this is
   * dispatched to (which should always be the calling thread).
   */
  CompletionNotifier(mozIStorageStatementCallback *aCallback,
                     ExecutionState aReason,
                     iCompletionNotifier *aCompletionNotifier) :
      mCallback(aCallback)
    , mReason(aReason)
    , mCompletionNotifier(aCompletionNotifier)
  {
  }

  NS_IMETHOD Run()
  {
    (void)mCallback->HandleCompletion(mReason);
    NS_RELEASE(mCallback);

    mCompletionNotifier->completed(this);
    // It is likely that the completion notifier holds a reference to us as
    // well, so we release our reference to it here to avoid cycles.
    mCompletionNotifier = nsnull;
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
  nsRefPtr<iCompletionNotifier> mCompletionNotifier;
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
                   , public iCompletionNotifier
{
public:
  NS_DECL_ISUPPORTS

  /**
   * This takes ownership of both the statement and the callback.
   */
  AsyncExecute(sqlite3_stmt *aStatement,
               mozIStorageStatementCallback *aCallback) :
      mStatement(aStatement)
    , mCallback(aCallback)
    , mCallingThread(do_GetCurrentThread())
    , mState(PENDING)
    , mStateMutex(nsAutoLock::NewLock("AsyncExecute::mStateMutex"))
    , mPendingEventsMutex(nsAutoLock::NewLock("AsyncExecute::mPendingEventsMutex"))
  {
  }

  nsresult initialize()
  {
    NS_ENSURE_TRUE(mStateMutex, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(mPendingEventsMutex, NS_ERROR_OUT_OF_MEMORY);
    NS_IF_ADDREF(mCallback);
    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    // do not run if we have been canceled
    {
      nsAutoLock mutex(mStateMutex);
      if (mState == CANCELED)
        return Complete();
    }

    // Execute the statement, giving the callback results
    // XXX better chunking of results?
    nsresult rv = NS_OK;
    while (PR_TRUE) {
      int rc = sqlite3_step(mStatement);
      // Break out if we have no more results
      if (rc == SQLITE_DONE)
        break;

      // Some errors are not fatal, and we can handle them and continue.
      if (rc != SQLITE_OK && rc != SQLITE_ROW) {
        if (rc == SQLITE_BUSY) {
          // Yield, and try again
          PR_Sleep(PR_INTERVAL_NO_WAIT);
          continue;
        }

        // Set error state
        {
          nsAutoLock mutex(mStateMutex);
          mState = ERROR;
        }

        // Notify
        sqlite3 *db = sqlite3_db_handle(mStatement);
        iCancelable *cancelable = ErrorNotifier::Dispatch(
          mCallingThread, mCallback, this, rc, sqlite3_errmsg(db)
        );
        if (cancelable) {
          nsAutoLock mutex(mPendingEventsMutex);
          (void)mPendingEvents.AppendObject(cancelable);
        }

        // And complete
        return Complete();
      }

      // Check to see if we have been canceled
      {
        nsAutoLock mutex(mStateMutex);
        if (mState == CANCELED)
          return Complete();
      }

      // If we do not have a callback, but are getting results, we should stop
      // now since all this work isn't going to accomplish anything
      if (!mCallback) {
        nsAutoLock mutex(mStateMutex);
        mState = COMPLETED;
        return Complete();
      }

      // Build result object
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

      rv = row->initialize(mStatement);
      if (NS_FAILED(rv)) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }

      rv = results->add(row);
      if (NS_FAILED(rv))
        break;

      // Notify caller
      nsRefPtr<CallbackResultNotifier> notifier =
        new CallbackResultNotifier(mCallback, results, this);
      if (!notifier) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }

      nsresult status = mCallingThread->Dispatch(notifier, NS_DISPATCH_NORMAL);
      if (NS_SUCCEEDED(status)) {
        nsAutoLock mutex(mPendingEventsMutex);
        (void)mPendingEvents.AppendObject(notifier);
      }
    }

    // We have broken out of the loop because of an error or because we are
    // completed.  Handle accordingly.
    if (NS_FAILED(rv)) {
      // This is a fatal error :(

      // Update state
      {
        nsAutoLock mutex(mStateMutex);
        mState = ERROR;
      }

      // Notify
      iCancelable *cancelable = ErrorNotifier::Dispatch(
        mCallingThread, mCallback, this, mozIStorageError::ERROR, ""
      );
      if (cancelable) {
        nsAutoLock mutex(mPendingEventsMutex);
        (void)mPendingEvents.AppendObject(cancelable);
      }
    }

    // No more results, so update state if needed
    {
      nsAutoLock mutex(mStateMutex);
      if (mState == PENDING)
        mState = COMPLETED;

      // Notify about completion
      return Complete();
    }
  }

  static PRBool cancelEnumerator(iCancelable *aCancelable, void *)
  {
    (void)aCancelable->cancel();
    return PR_TRUE;
  }

  NS_IMETHOD Cancel()
  {
    // Check and update our state
    {
      nsAutoLock mutex(mStateMutex);
      NS_ENSURE_TRUE(mState == PENDING || mState == COMPLETED,
                     NS_ERROR_UNEXPECTED);
      mState = CANCELED;
    }

    // Cancel all our pending events on the calling thread
    {
      nsAutoLock mutex(mPendingEventsMutex);
      (void)mPendingEvents.EnumerateForwards(&AsyncExecute::cancelEnumerator,
                                             nsnull);
      mPendingEvents.Clear();
    }

    return NS_OK;
  }

  virtual void completed(iCancelable *aCancelable)
  {
    nsAutoLock mutex(mPendingEventsMutex);
    (void)mPendingEvents.RemoveObject(aCancelable);
  }

private:
  AsyncExecute() { }

  ~AsyncExecute()
  {
    NS_ASSERTION(mPendingEvents.Count() == 0, "Still pending events!");
    nsAutoLock::DestroyLock(mStateMutex);
    nsAutoLock::DestroyLock(mPendingEventsMutex);
  }

  /**
   * Notifies callback about completion, and does any necessary cleanup.
   * @note: When calling this function, mStateMutex must be held.
   */
  nsresult Complete()
  {
    NS_ASSERTION(mState != PENDING,
                 "Still in a pending state when calling Complete!");

    // Reset the statement
    (void)sqlite3_finalize(mStatement);
    mStatement = NULL;

    // Notify about completion iff we have a callback.
    if (mCallback) {
      nsRefPtr<CompletionNotifier> completionEvent =
        new CompletionNotifier(mCallback, mState, this);
      nsresult rv = mCallingThread->Dispatch(completionEvent, NS_DISPATCH_NORMAL);
      if (NS_SUCCEEDED(rv)) {
        nsAutoLock mutex(mPendingEventsMutex);
        (void)mPendingEvents.AppendObject(completionEvent);
      }

      // We no longer own mCallback (the CompletionNotifier takes ownership).
      mCallback = nsnull;
    }

    return NS_OK;
  }

  sqlite3_stmt *mStatement;
  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<nsIThread> mCallingThread;

  /**
   * Indicates the state the object is currently in.
   */
  ExecutionState mState;

  /**
   * Mutex to protect mState.
   */
  PRLock *mStateMutex;

  /**
   * Stores a list of pending events that have not yet completed on the
   * calling thread.
   */
  nsCOMArray<iCancelable> mPendingEvents;

  /**
   * Mutex to protect mPendingEvents.
   */
  PRLock *mPendingEventsMutex;
};
NS_IMPL_THREADSAFE_ISUPPORTS2(
  AsyncExecute,
  nsIRunnable,
  mozIStoragePendingStatement
)

nsresult
NS_executeAsync(sqlite3_stmt *aStatement,
                mozIStorageStatementCallback *aCallback,
                mozIStoragePendingStatement **_stmt)
{
  // Create our event to run in the background
  nsRefPtr<AsyncExecute> event(new AsyncExecute(aStatement, aCallback));
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
