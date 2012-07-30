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
#include "nsIInterfaceRequestor.h"

#include "nsDataHashtable.h"
#include "mozIStorageProgressHandler.h"
#include "SQLiteMutex.h"
#include "mozIStorageConnection.h"
#include "mozStorageService.h"

#include "nsIMutableArray.h"
#include "mozilla/Attributes.h"

#include "sqlite3.h"

struct PRLock;
class nsIFile;
class nsIEventTarget;
class nsIThread;

namespace mozilla {
namespace storage {

class Connection MOZ_FINAL : public mozIStorageConnection
                           , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
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
    PRInt32 numArgs;
  };

  /**
   * @param aService
   *        Pointer to the storage service.  Held onto for the lifetime of the
   *        connection.
   * @param aFlags
   *        The flags to pass to sqlite3_open_v2.
   */
  Connection(Service *aService, int aFlags);

  /**
   * Creates the connection to the database.
   *
   * @param aDatabaseFile
   *        The nsIFile of the location of the database to open, or create if it
   *        does not exist.  Passing in nullptr here creates an in-memory
   *        database.
   * @param aVFSName
   *        The VFS that SQLite will use when opening this database. NULL means
   *        "default".
   */
  nsresult initialize(nsIFile *aDatabaseFile,
                      const char* aVFSName = NULL);

  // fetch the native handle
  sqlite3 *GetNativeConnection() { return mDBConn; }
  operator sqlite3 *() const { return mDBConn; }

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
   * asynchronous statements (they are serialized on mAsyncExecutionThread).  It
   * also protects mPendingStatements.
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
  nsresult internalClose();

  /**
   * Obtains the filename of the connection.  Useful for logging.
   */
  nsCString getFilename();

  /**
   * Creates an sqlite3 prepared statement object from an SQL string.
   *
   * @param aSQL
   *        The SQL statement string to compile.
   * @param _stmt
   *        New sqlite3_stmt object.
   * @return the result from sqlite3_prepare_v2.
   */
  int prepareStatement(const nsCString &aSQL, sqlite3_stmt **_stmt);

  /**
   * Performs a sqlite3_step on aStatement, while properly handling SQLITE_LOCKED
   * when not on the main thread by waiting until we are notified.
   *
   * @param aStatement
   *        A pointer to a sqlite3_stmt object.
   * @return the result from sqlite3_step.
   */
  int stepStatement(sqlite3_stmt* aStatement);

  bool ConnectionReady() {
    return mDBConn != nullptr;
  }

  /**
   * True if this is an async connection, it is shutting down and it is not
   * closed yet.
   */
  bool isAsyncClosing();

private:
  ~Connection();

  /**
   * Sets the database into a closed state so no further actions can be
   * performed.
   *
   * @note mDBConn is set to NULL in this method.
   */
  nsresult setClosedState();

  /**
   * Helper for calls to sqlite3_exec. Reports long delays to Telemetry.
   *
   * @param aSqlString
   *        SQL string to execute
   * @return the result from sqlite3_exec.
   */
  int executeSql(const char *aSqlString);

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
  nsCOMPtr<nsIFile> mDatabaseFile;

  /**
   * Lazily created thread for asynchronous statement execution.  Consumers
   * should use getAsyncExecutionTarget rather than directly accessing this
   * field.
   */
  nsCOMPtr<nsIThread> mAsyncExecutionThread;
  /**
   * Set to true by Close() prior to actually shutting down the thread.  This
   * lets getAsyncExecutionTarget() know not to hand out any more thread
   * references (or to create the thread in the first place).  This variable
   * should be accessed while holding the mAsyncExecutionMutex.
   */
  bool mAsyncExecutionThreadShuttingDown;

  /**
   * Tracks if we have a transaction in progress or not.  Access protected by
   * mDBMutex.
   */
  bool mTransactionInProgress;

  /**
   * Stores the mapping of a given function by name to its instance.  Access is
   * protected by mDBMutex.
   */
  nsDataHashtable<nsCStringHashKey, FunctionInfo> mFunctions;

  /**
   * Stores the registered progress handler for the database connection.  Access
   * is protected by mDBMutex.
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
};

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_Connection_h
