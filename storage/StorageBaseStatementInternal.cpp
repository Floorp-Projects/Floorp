/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageBaseStatementInternal.h"

#include "nsProxyRelease.h"

#include "mozStorageBindingParamsArray.h"
#include "mozStorageStatementData.h"
#include "mozStorageAsyncStatementExecution.h"

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Local Classes

/**
 * Used to finalize an asynchronous statement on the background thread.
 */
class AsyncStatementFinalizer : public Runnable {
 public:
  /**
   * Constructor for the event.
   *
   * @param aStatement
   *        We need the AsyncStatement to be able to get at the sqlite3_stmt;
   *        we only access/create it on the async thread.
   * @param aConnection
   *        We need the connection to know what thread to release the statement
   *        on.  We release the statement on that thread since releasing the
   *        statement might end up releasing the connection too.
   */
  AsyncStatementFinalizer(StorageBaseStatementInternal* aStatement,
                          Connection* aConnection)
      : Runnable("storage::AsyncStatementFinalizer"),
        mStatement(aStatement),
        mConnection(aConnection) {}

  NS_IMETHOD Run() override {
    if (mStatement->mAsyncStatement) {
      sqlite3_finalize(mStatement->mAsyncStatement);
      mStatement->mAsyncStatement = nullptr;
    }

    nsCOMPtr<nsIThread> targetThread(mConnection->threadOpenedOn);
    NS_ProxyRelease("AsyncStatementFinalizer::mStatement", targetThread,
                    mStatement.forget());
    return NS_OK;
  }

 private:
  RefPtr<StorageBaseStatementInternal> mStatement;
  RefPtr<Connection> mConnection;
};

/**
 * Finalize a sqlite3_stmt on the background thread for a statement whose
 * destructor was invoked and the statement was non-null.
 */
class LastDitchSqliteStatementFinalizer : public Runnable {
 public:
  /**
   * Event constructor.
   *
   * @param aConnection
   *        Used to keep the connection alive.  If we failed to do this, it
   *        is possible that the statement going out of scope invoking us
   *        might have the last reference to the connection and so trigger
   *        an attempt to close the connection which is doomed to fail
   *        (because the asynchronous execution thread must exist which will
   *        trigger the failure case).
   * @param aStatement
   *        The sqlite3_stmt to finalize.  This object takes ownership /
   *        responsibility for the instance and all other references to it
   *        should be forgotten.
   */
  LastDitchSqliteStatementFinalizer(RefPtr<Connection>& aConnection,
                                    sqlite3_stmt* aStatement)
      : Runnable("storage::LastDitchSqliteStatementFinalizer"),
        mConnection(aConnection),
        mAsyncStatement(aStatement) {
    MOZ_ASSERT(aConnection, "You must provide a Connection");
  }

  NS_IMETHOD Run() override {
    (void)::sqlite3_finalize(mAsyncStatement);
    mAsyncStatement = nullptr;

    nsCOMPtr<nsIThread> target(mConnection->threadOpenedOn);
    (void)::NS_ProxyRelease("LastDitchSqliteStatementFinalizer::mConnection",
                            target, mConnection.forget());
    return NS_OK;
  }

 private:
  RefPtr<Connection> mConnection;
  sqlite3_stmt* mAsyncStatement;
};

////////////////////////////////////////////////////////////////////////////////
//// StorageBaseStatementInternal

StorageBaseStatementInternal::StorageBaseStatementInternal()
    : mNativeConnection(nullptr), mAsyncStatement(nullptr) {}

void StorageBaseStatementInternal::asyncFinalize() {
  nsIEventTarget* target = mDBConnection->getAsyncExecutionTarget();
  if (target) {
    // Attempt to finalize asynchronously
    nsCOMPtr<nsIRunnable> event =
        new AsyncStatementFinalizer(this, mDBConnection);

    // Dispatch. Note that dispatching can fail, typically if
    // we have a race condition with asyncClose(). It's ok,
    // let asyncClose() win.
    (void)target->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  // If we cannot get the background thread,
  // mozStorageConnection::AsyncClose() has already been called and
  // the statement either has been or will be cleaned up by
  // internalClose().
}

void StorageBaseStatementInternal::destructorAsyncFinalize() {
  if (!mAsyncStatement) return;

  bool isOwningThread = false;
  (void)mDBConnection->threadOpenedOn->IsOnCurrentThread(&isOwningThread);
  if (isOwningThread) {
    // If we are the owning thread (currently that means we're also the
    // main thread), then we can get the async target and just dispatch
    // to it.
    nsIEventTarget* target = mDBConnection->getAsyncExecutionTarget();
    if (target) {
      nsCOMPtr<nsIRunnable> event =
          new LastDitchSqliteStatementFinalizer(mDBConnection, mAsyncStatement);
      (void)target->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  } else {
    // If we're not the owning thread, assume we're the async thread, and
    // just run the statement.
    nsCOMPtr<nsIRunnable> event =
        new LastDitchSqliteStatementFinalizer(mDBConnection, mAsyncStatement);
    (void)event->Run();
  }

  // We might not be able to dispatch to the background thread,
  // presumably because it is being shutdown. Since said shutdown will
  // finalize the statement, we just need to clean-up around here.
  mAsyncStatement = nullptr;
}

NS_IMETHODIMP
StorageBaseStatementInternal::NewBindingParamsArray(
    mozIStorageBindingParamsArray** _array) {
  nsCOMPtr<mozIStorageBindingParamsArray> array = new BindingParamsArray(this);
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  array.forget(_array);
  return NS_OK;
}

NS_IMETHODIMP
StorageBaseStatementInternal::ExecuteAsync(
    mozIStorageStatementCallback* aCallback,
    mozIStoragePendingStatement** _stmt) {
  // We used to call Connection::ExecuteAsync but it takes a
  // mozIStorageBaseStatement signature because it is also a public API.  Since
  // our 'this' has no static concept of mozIStorageBaseStatement and Connection
  // would just QI it back across to a StorageBaseStatementInternal and the
  // actual logic is very simple, we now roll our own.
  nsTArray<StatementData> stmts(1);
  StatementData data;
  nsresult rv = getAsynchronousStatementData(data);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(stmts.AppendElement(data), NS_ERROR_OUT_OF_MEMORY);

  // Dispatch to the background
  return AsyncExecuteStatements::execute(stmts, mDBConnection,
                                         mNativeConnection, aCallback, _stmt);
}

NS_IMETHODIMP
StorageBaseStatementInternal::EscapeStringForLIKE(const nsAString& aValue,
                                                  const char16_t aEscapeChar,
                                                  nsAString& _escapedString) {
  const char16_t MATCH_ALL('%');
  const char16_t MATCH_ONE('_');

  _escapedString.Truncate(0);

  for (uint32_t i = 0; i < aValue.Length(); i++) {
    if (aValue[i] == aEscapeChar || aValue[i] == MATCH_ALL ||
        aValue[i] == MATCH_ONE) {
      _escapedString += aEscapeChar;
    }
    _escapedString += aValue[i];
  }
  return NS_OK;
}

}  // namespace storage
}  // namespace mozilla
