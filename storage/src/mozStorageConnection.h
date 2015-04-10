/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_Connection_h
#define mozilla_storage_Connection_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "mozilla/Mutex.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestor.h"

#include "nsDataHashtable.h"
#include "mozIStorageProgressHandler.h"
#include "SQLiteMutex.h"
#include "mozIStorageConnection.h"
#include "mozStorageService.h"
#include "mozIStorageAsyncConnection.h"
#include "mozIStorageCompletionCallback.h"

#include "nsIMutableArray.h"
#include "mozilla/Attributes.h"

#include "sqlite3.h"

struct PRLock;
class nsIFile;
class nsIFileURL;
class nsIEventTarget;
class nsIThread;

namespace mozilla {
namespace storage {

class Connection final : public mozIStorageConnection
                       , public nsIInterfaceRequestor
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEASYNCCONNECTION
  NS_DECL_MOZISTORAGECONNECTION
  NS_DECL_NSIINTERFACEREQUESTOR

  /**
   * Structure used to describe user functions on the database connection.
   */
  struct FunctionInfo {
    enum FunctionType {
      SIMPLE,
      AGGREGATE
    };

    nsCOMPtr<nsISupports> function;
    FunctionType type;
    int32_t numArgs;
  };

  /**
   * @param aService
   *        Pointer to the storage service.  Held onto for the lifetime of the
   *        connection.
   * @param aFlags
   *        The flags to pass to sqlite3_open_v2.
   * @param aAsyncOnly
   *        If |true|, the Connection only implements asynchronous interface:
   *        - |mozIStorageAsyncConnection|;
   *        If |false|, the result also implements synchronous interface:
   *        - |mozIStorageConnection|.
   */
  Connection(Service *aService, int aFlags, bool aAsyncOnly);

  /**
   * Creates the connection to an in-memory database.
   */
  nsresult initialize();

  /**
   * Creates the connection to the database.
   *
   * @param aDatabaseFile
   *        The nsIFile of the location of the database to open, or create if it
   *        does not exist.
   */
  nsresult initialize(nsIFile *aDatabaseFile);

  /**
   * Creates the connection to the database.
   *
   * @param aFileURL
   *        The nsIFileURL of the location of the database to open, or create if it
   *        does not exist.
   */
  nsresult initialize(nsIFileURL *aFileURL);

  /**
   * Fetches runtime status information for this connection.
   *
   * @param aStatusOption One of the SQLITE_DBSTATUS options defined at
   *        http://www.sqlite.org/c3ref/c_dbstatus_options.html
   * @param [optional] aMaxValue if provided, will be set to the highest
   *        istantaneous value.
   * @return the current value for the specified option.
   */
  int32_t getSqliteRuntimeStatus(int32_t aStatusOption,
                                 int32_t* aMaxValue=nullptr);
  /**
   * Registers/unregisters a commit hook callback.
   *
   * @param aCallbackFn a callback function to be invoked on transactions
   *        commit.  Pass nullptr to unregister the current callback.
   * @param [optional] aData if provided, will be passed to the callback.
   * @see http://sqlite.org/c3ref/commit_hook.html
   */
  void setCommitHook(int (*aCallbackFn)(void *) , void *aData=nullptr) {
    MOZ_ASSERT(mDBConn, "A connection must exist at this point");
    ::sqlite3_commit_hook(mDBConn, aCallbackFn, aData);
  };

  /**
   * Gets autocommit status.
   */
  bool getAutocommit() {
    return mDBConn && static_cast<bool>(::sqlite3_get_autocommit(mDBConn));
  };

  /**
   * Lazily creates and returns a background execution thread.  In the future,
   * the thread may be re-claimed if left idle, so you should call this
   * method just before you dispatch and not save the reference.
   *
   * @returns an event target suitable for asynchronous statement execution.
   */
  nsIEventTarget *getAsyncExecutionTarget();

  /**
   * Mutex used by asynchronous statements to protect state.  The mutex is
   * declared on the connection object because there is no contention between
   * asynchronous statements (they are serialized on mAsyncExecutionThread).
   * Currently protects:
   *  - Connection.mAsyncExecutionThreadShuttingDown
   *  - Connection.mAsyncExecutionThread
   *  - Connection.mConnectionClosed
   *  - AsyncExecuteStatements.mCancelRequested
   */
  Mutex sharedAsyncExecutionMutex;

  /**
   * Wraps the mutex that SQLite gives us from sqlite3_db_mutex.  This is public
   * because we already expose the sqlite3* native connection and proper
   * operation of the deadlock detector requires everyone to use the same single
   * SQLiteMutex instance for correctness.
   */
  SQLiteMutex sharedDBMutex;

  /**
   * References the thread this database was opened on.  This MUST be thread it is
   * closed on.
   */
  const nsCOMPtr<nsIThread> threadOpenedOn;

  /**
   * Closes the SQLite database, and warns about any non-finalized statements.
   */
  nsresult internalClose(sqlite3 *aDBConn);

  /**
   * Obtains the filename of the connection.  Useful for logging.
   */
  nsCString getFilename();

  /**
   * Creates an sqlite3 prepared statement object from an SQL string.
   *
   * @param aNativeConnection
   *        The underlying Sqlite connection to prepare the statement with.
   * @param aSQL
   *        The SQL statement string to compile.
   * @param _stmt
   *        New sqlite3_stmt object.
   * @return the result from sqlite3_prepare_v2.
   */
  int prepareStatement(sqlite3* aNativeConnection,
                       const nsCString &aSQL, sqlite3_stmt **_stmt);

  /**
   * Performs a sqlite3_step on aStatement, while properly handling SQLITE_LOCKED
   * when not on the main thread by waiting until we are notified.
   *
   * @param aNativeConnection
   *        The underlying Sqlite connection to step the statement with.
   * @param aStatement
   *        A pointer to a sqlite3_stmt object.
   * @return the result from sqlite3_step.
   */
  int stepStatement(sqlite3* aNativeConnection, sqlite3_stmt* aStatement);

  /**
   * Raw connection transaction management.
   *
   * @see BeginTransactionAs, CommitTransaction, RollbackTransaction.
   */
  nsresult beginTransactionInternal(sqlite3 *aNativeConnection,
                                    int32_t aTransactionType=TRANSACTION_DEFERRED);
  nsresult commitTransactionInternal(sqlite3 *aNativeConnection);
  nsresult rollbackTransactionInternal(sqlite3 *aNativeConnection);

  bool connectionReady();

  /**
   * True if this connection is shutting down but not yet closed.
   */
  bool isClosing();

  /**
   * True if the underlying connection is closed.
   * Any sqlite resources may be lost when this returns true, so nothing should
   * try to use them.
   */
  bool isClosed();

  nsresult initializeClone(Connection *aClone, bool aReadOnly);

private:
  ~Connection();
  nsresult initializeInternal(nsIFile *aDatabaseFile);

  /**
   * Sets the database into a closed state so no further actions can be
   * performed.
   *
   * @note mDBConn is set to nullptr in this method.
   */
  nsresult setClosedState();

  /**
   * Helper for calls to sqlite3_exec. Reports long delays to Telemetry.
   *
   * @param aNativeConnection
   *        The underlying Sqlite connection to execute the query with.
   * @param aSqlString
   *        SQL string to execute
   * @return the result from sqlite3_exec.
   */
  int executeSql(sqlite3 *aNativeConnection, const char *aSqlString);

  /**
   * Describes a certain primitive type in the database.
   *
   * Possible Values Are:
   *  INDEX - To check for the existence of an index
   *  TABLE - To check for the existence of a table
   */
  enum DatabaseElementType {
    INDEX,
    TABLE
  };

  /**
   * Determines if the specified primitive exists.
   *
   * @param aElementType
   *        The type of element to check the existence of
   * @param aElementName
   *        The name of the element to check for
   * @returns true if element exists, false otherwise
   */
  nsresult databaseElementExists(enum DatabaseElementType aElementType,
                                 const nsACString& aElementName,
                                 bool *_exists);

  bool findFunctionByInstance(nsISupports *aInstance);

  static int sProgressHelper(void *aArg);
  // Generic progress handler
  // Dispatch call to registered progress handler,
  // if there is one. Do nothing in other cases.
  int progressHandler();

  sqlite3 *mDBConn;
  nsCOMPtr<nsIFileURL> mFileURL;
  nsCOMPtr<nsIFile> mDatabaseFile;

  /**
   * Lazily created thread for asynchronous statement execution.  Consumers
   * should use getAsyncExecutionTarget rather than directly accessing this
   * field.
   */
  nsCOMPtr<nsIThread> mAsyncExecutionThread;

  /**
   * Set to true by Close() or AsyncClose() prior to shutdown.
   *
   * If false, we guarantee both that the underlying sqlite3 database
   * connection is still open and that getAsyncExecutionTarget() can
   * return a thread. Once true, either the sqlite3 database
   * connection is being shutdown or it has been
   * shutdown. Additionally, once true, getAsyncExecutionTarget()
   * returns null.
   *
   * This variable should be accessed while holding the
   * sharedAsyncExecutionMutex.
   */
  bool mAsyncExecutionThreadShuttingDown;

  /**
   * Set to true just prior to calling sqlite3_close on the
   * connection.
   *
   * This variable should be accessed while holding the
   * sharedAsyncExecutionMutex.
   */
  bool mConnectionClosed;

  /**
   * Tracks if we have a transaction in progress or not.  Access protected by
   * sharedDBMutex.
   */
  bool mTransactionInProgress;

  /**
   * Stores the mapping of a given function by name to its instance.  Access is
   * protected by sharedDBMutex.
   */
  nsDataHashtable<nsCStringHashKey, FunctionInfo> mFunctions;

  /**
   * Stores the registered progress handler for the database connection.  Access
   * is protected by sharedDBMutex.
   */
  nsCOMPtr<mozIStorageProgressHandler> mProgressHandler;

  /**
   * Stores the flags we passed to sqlite3_open_v2.
   */
  const int mFlags;

  // This is here for two reasons: 1) It's used to make sure that the
  // connections do not outlive the service.  2) Our custom collating functions
  // call its localeCompareStrings() method.
  nsRefPtr<Service> mStorageService;

  /**
   * If |false|, this instance supports synchronous operations
   * and it can be cast to |mozIStorageConnection|.
   */
  const bool mAsyncOnly;
};


/**
 * A Runnable designed to call a mozIStorageCompletionCallback on
 * the appropriate thread.
 */
class CallbackComplete final : public nsRunnable
{
public:
  /**
   * @param aValue The result to pass to the callback. It must
   *               already be owned by the main thread.
   * @param aCallback The callback. It must already be owned by the
   *                  main thread.
   */
  CallbackComplete(nsresult aStatus,
                   nsISupports* aValue,
                   already_AddRefed<mozIStorageCompletionCallback> aCallback)
    : mStatus(aStatus)
    , mValue(aValue)
    , mCallback(aCallback)
  {
  }

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    nsresult rv = mCallback->Complete(mStatus, mValue);

    // Ensure that we release on the main thread
    mValue = nullptr;
    mCallback = nullptr;
    return rv;
  }

private:
  nsresult mStatus;
  nsCOMPtr<nsISupports> mValue;
  // This is a nsRefPtr<T> and not a nsCOMPtr<T> because
  // nsCOMP<T> would cause an off-main thread QI, which
  // is not a good idea (and crashes XPConnect).
  nsRefPtr<mozIStorageCompletionCallback> mCallback;
};

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_Connection_h
