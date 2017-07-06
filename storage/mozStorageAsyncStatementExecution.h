/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageAsyncStatementExecution_h
#define mozStorageAsyncStatementExecution_h

#include "nscore.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"
#include "nsIRunnable.h"

#include "SQLiteMutex.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozStorageHelper.h"

struct sqlite3_stmt;

namespace mozilla {
namespace storage {

class Connection;
class ResultSet;
class StatementData;

class AsyncExecuteStatements final : public nsIRunnable
                                   , public mozIStoragePendingStatement
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_MOZISTORAGEPENDINGSTATEMENT

  /**
   * Describes the state of execution.
   */
  enum ExecutionState {
    PENDING = -1,
    COMPLETED = mozIStorageStatementCallback::REASON_FINISHED,
    CANCELED = mozIStorageStatementCallback::REASON_CANCELED,
    ERROR = mozIStorageStatementCallback::REASON_ERROR
  };

  typedef nsTArray<StatementData> StatementDataArray;

  /**
   * Executes a statement in the background, and passes results back to the
   * caller.
   *
   * @param aStatements
   *        The statements to execute and possibly bind in the background.
   *        Ownership is transfered from the caller.
   * @param aConnection
   *        The connection that created the statements to execute.
   * @param aNativeConnection
   *        The native Sqlite connection that created the statements to execute.
   * @param aCallback
   *        The callback that is notified of results, completion, and errors.
   * @param _stmt
   *        The handle to control the execution of the statements.
   */
  static nsresult execute(StatementDataArray &aStatements,
                          Connection *aConnection,
                          sqlite3 *aNativeConnection,
                          mozIStorageStatementCallback *aCallback,
                          mozIStoragePendingStatement **_stmt);

  /**
   * Indicates when events on the calling thread should run or not.  Certain
   * events posted back to the calling thread should call this see if they
   * should run or not.
   *
   * @pre mMutex is not held
   *
   * @returns true if the event should notify still, false otherwise.
   */
  bool shouldNotify();

  /**
   * Used by notifyComplete(), notifyError() and notifyResults() to notify on
   * the calling thread.
   */
  nsresult notifyCompleteOnCallingThread();
  nsresult notifyErrorOnCallingThread(mozIStorageError *aError);
  nsresult notifyResultsOnCallingThread(ResultSet *aResultSet);

private:
  AsyncExecuteStatements(StatementDataArray &aStatements,
                         Connection *aConnection,
                         sqlite3 *aNativeConnection,
                         mozIStorageStatementCallback *aCallback);
  ~AsyncExecuteStatements();

  /**
   * Binds and then executes a given statement until completion, an error
   * occurs, or we are canceled.  If aLastStatement is true, we should set
   * mState accordingly.
   *
   * @pre mMutex is not held
   *
   * @param aData
   *        The StatementData to bind, execute, and then process.
   * @param aLastStatement
   *        Indicates if this is the last statement or not.  If it is, we have
   *        to set the proper state.
   * @returns true if we should continue to process statements, false otherwise.
   */
  bool bindExecuteAndProcessStatement(StatementData &aData,
                                      bool aLastStatement);

  /**
   * Executes a given statement until completion, an error occurs, or we are
   * canceled.  If aLastStatement is true, we should set mState accordingly.
   *
   * @pre mMutex is not held
   *
   * @param aStatement
   *        The statement to execute and then process.
   * @param aLastStatement
   *        Indicates if this is the last statement or not.  If it is, we have
   *        to set the proper state.
   * @returns true if we should continue to process statements, false otherwise.
   */
  bool executeAndProcessStatement(sqlite3_stmt *aStatement,
                                  bool aLastStatement);

  /**
   * Executes a statement to completion, properly handling any error conditions.
   *
   * @pre mMutex is not held
   *
   * @param aStatement
   *        The statement to execute to completion.
   * @returns true if results were obtained, false otherwise.
   */
  bool executeStatement(sqlite3_stmt *aStatement);

  /**
   * Builds a result set up with a row from a given statement.  If we meet the
   * right criteria, go ahead and notify about this results too.
   *
   * @pre mMutex is not held
   *
   * @param aStatement
   *        The statement to get the row data from.
   */
  nsresult buildAndNotifyResults(sqlite3_stmt *aStatement);

  /**
   * Notifies callback about completion, and does any necessary cleanup.
   *
   * @pre mMutex is not held
   */
  nsresult notifyComplete();

  /**
   * Notifies callback about an error.
   *
   * @pre mMutex is not held
   * @pre mDBMutex is not held
   *
   * @param aErrorCode
   *        The error code defined in mozIStorageError for the error.
   * @param aMessage
   *        The error string, if any.
   * @param aError
   *        The error object to notify the caller with.
   */
  nsresult notifyError(int32_t aErrorCode, const char *aMessage);
  nsresult notifyError(mozIStorageError *aError);

  /**
   * Notifies the callback about a result set.
   *
   * @pre mMutex is not held
   */
  nsresult notifyResults();

  /**
   * Tests whether the current statements should be wrapped in an explicit
   * transaction.
   *
   * @return true if an explicit transaction is needed, false otherwise.
   */
  bool statementsNeedTransaction();

  StatementDataArray mStatements;
  RefPtr<Connection> mConnection;
  sqlite3 *mNativeConnection;
  bool mHasTransaction;
  // Note, this may not be a threadsafe object - never addref/release off
  // the calling thread.  We take a reference when this is created, and
  // release it in the CompletionNotifier::Run() call back to this thread.
  nsCOMPtr<mozIStorageStatementCallback> mCallback;
  nsCOMPtr<nsIThread> mCallingThread;
  RefPtr<ResultSet> mResultSet;

  /**
   * The maximum amount of time we want to wait between results.  Defined by
   * MAX_MILLISECONDS_BETWEEN_RESULTS and set at construction.
   */
  const TimeDuration mMaxWait;

  /**
   * The start time since our last set of results.
   */
  TimeStamp mIntervalStart;

  /**
   * Indicates our state of execution.
   */
  ExecutionState mState;

  /**
   * Indicates if we should try to cancel at a cancelation point.
   */
  bool mCancelRequested;

  /**
   * This is the mutex that protects our state from changing between threads.
   * This includes the following variables:
   *   - mCancelRequested is only set on the calling thread while the lock is
   *     held.  It is always read from within the lock on the background thread,
   *     but not on the calling thread (see shouldNotify for why).
   */
  Mutex &mMutex;

  /**
   * The wrapped SQLite recursive connection mutex.  We use it whenever we call
   * sqlite3_step and care about having reliable error messages.  By taking it
   * prior to the call and holding it until the point where we no longer care
   * about the error message, the user gets reliable error messages.
   */
  SQLiteMutex &mDBMutex;

  /**
   * The instant at which the request was started.
   *
   * Used by telemetry.
   */
  TimeStamp mRequestStartDate;
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageAsyncStatementExecution_h
