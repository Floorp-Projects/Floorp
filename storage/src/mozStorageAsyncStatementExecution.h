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

#ifndef _mozStorageAsyncStatementExecution_h_
#define _mozStorageAsyncStatementExecution_h_

#include "nscore.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"

#include "mozIStoragePendingStatement.h"
#include "mozIStorageStatementCallback.h"

struct sqlite3_stmt;
class mozStorageTransaction;

namespace mozilla {
namespace storage {

class Connection;
class ResultSet;
class StatementData;

class AsyncExecuteStatements : public nsIRunnable
                             , public mozIStoragePendingStatement
{
public:
  NS_DECL_ISUPPORTS
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
   * @param aCallback
   *        The callback that is notified of results, completion, and errors.
   * @param _stmt
   *        The handle to control the execution of the statements.
   */
  static nsresult execute(StatementDataArray &aStatements,
                          Connection *aConnection,
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

private:
  AsyncExecuteStatements(StatementDataArray &aStatements,
                         Connection *aConnection,
                         mozIStorageStatementCallback *aCallback);

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
   *
   * @param aErrorCode
   *        The error code defined in mozIStorageError for the error.
   * @param aMessage
   *        The error string, if any.
   * @param aError
   *        The error object to notify the caller with.
   */
  nsresult notifyError(PRInt32 aErrorCode, const char *aMessage);
  nsresult notifyError(mozIStorageError *aError);

  /**
   * Notifies the callback about a result set.
   *
   * @pre mMutex is not held
   */
  nsresult notifyResults();

  StatementDataArray mStatements;
  nsRefPtr<Connection> mConnection;
  mozStorageTransaction *mTransactionManager;
  mozIStorageStatementCallback *mCallback;
  nsCOMPtr<nsIThread> mCallingThread;
  nsRefPtr<ResultSet> mResultSet;

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
};

} // namespace storage
} // namespace mozilla

#endif // _mozStorageAsyncStatementExecution_h_
