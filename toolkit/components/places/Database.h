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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

#ifndef mozilla_places_Database_h_
#define mozilla_places_Database_h_

#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "mozilla/storage.h"
#include "mozilla/storage/StatementCache.h"

// This is the schema version, update it at any schema change and add a
// corresponding migrateVxx method below.
#define DATABASE_SCHEMA_VERSION 12

// Fired after Places inited.
#define TOPIC_PLACES_INIT_COMPLETE "places-init-complete"
// Fired when initialization fails due to a locked database.
#define TOPIC_DATABASE_LOCKED "places-database-locked"
// This topic is received when the profile is about to be lost.  Places does
// initial shutdown work and notifies TOPIC_PLACES_SHUTDOWN to all listeners.
// Any shutdown work that requires the Places APIs should happen here.
#define TOPIC_PROFILE_CHANGE_TEARDOWN "profile-change-teardown"
// This topic is received just before the profile is lost.  Places begins
// shutting down the connection and notifies TOPIC_PLACES_WILL_CLOSE_CONNECTION
// to all listeners.  Only critical database cleanups should happen here,
// some APIs may bail out already.
#define TOPIC_PROFILE_BEFORE_CHANGE "profile-before-change"
// Fired when Places is shutting down.  Any code should stop accessing Places
// APIs after this notification.  If you need to listen for Places shutdown
// you should only use this notification, next ones are intended only for
// internal Places use.
#define TOPIC_PLACES_SHUTDOWN "places-shutdown"
// For Internal use only.  Fired when connection is about to be closed, only
// cleanup tasks should run at this stage, nothing should be added to the
// database, nor APIs should be called.
#define TOPIC_PLACES_WILL_CLOSE_CONNECTION "places-will-close-connection"
// Fired when the connection has gone, nothing will work from now on.
#define TOPIC_PLACES_CONNECTION_CLOSED "places-connection-closed"

namespace mozilla {
namespace places {

enum JournalMode {
  // Default SQLite journal mode.
  JOURNAL_DELETE = 0
  // Can reduce fsyncs on Linux when journal is deleted (See bug 460315).
  // We fallback to this mode when WAL is unavailable.
, JOURNAL_TRUNCATE
  // Unsafe in case of crashes on database swap or low memory.
, JOURNAL_MEMORY
  // Can reduce number of fsyncs.  We try to use this mode by default.
, JOURNAL_WAL
};

class Database : public nsIObserver
               , public nsSupportsWeakReference
{
  typedef mozilla::storage::StatementCache<mozIStorageStatement> StatementCache;
  typedef mozilla::storage::StatementCache<mozIStorageAsyncStatement> AsyncStatementCache;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  Database();

  /**
   * Initializes the database connection and the schema.
   * In case of corruption the database is copied to a backup file and replaced.
   */
  nsresult Init();

  /**
   * Finalizes the cached statements and closes the database connection.
   * A TOPIC_PLACES_CONNECTION_CLOSED notification is fired when done.
   */
  void Shutdown();

  /**
   * Getter to use when instantiating the class.
   *
   * @return Singleton instance of this class.
   */
  static already_AddRefed<Database> GetDatabase()
  {
    return GetSingleton();
  }

  /**
   * Returns last known database status.
   *
   * @return one of the nsINavHistoryService::DATABASE_STATUS_* constants.
   */
  PRUint16 GetDatabaseStatus() const
  {
    return mDatabaseStatus;
  }

  /**
   * Returns a pointer to the storage connection.
   *
   * @return The connection handle.
   */
  mozIStorageConnection* MainConn() const
  {
    return mMainConn;
  }

  /**
   * Dispatches a runnable to the connection async thread, to be serialized
   * with async statements.
   *
   * @param aEvent
   *        The runnable to be dispatched.
   */
  void DispatchToAsyncThread(nsIRunnable* aEvent) const
  {
    if (mShuttingDown) {
      return;
    }
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(mMainConn);
    if (target) {
      (void)target->Dispatch(aEvent, NS_DISPATCH_NORMAL);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  //// Statements Getters.

  /**
   * Gets a cached synchronous statement.
   *
   * @param aQuery
   *        SQL query literal.
   * @return The cached statement.
   * @note Always null check the result.
   * @note Always use a scoper to reset the statement.
   */
  template<int N>
  already_AddRefed<mozIStorageStatement>
  GetStatement(const char (&aQuery)[N]) const
  {
    nsDependentCString query(aQuery, N - 1);
    return GetStatement(query);
  }

  /**
   * Gets a cached synchronous statement.
   *
   * @param aQuery
   *        nsCString of SQL query.
   * @return The cached statement.
   * @note Always null check the result.
   * @note Always use a scoper to reset the statement.
   */
  already_AddRefed<mozIStorageStatement>
  GetStatement(const nsACString& aQuery) const
  {
    if (mShuttingDown) {
      return nsnull;
    }
    if (NS_IsMainThread()) {
      return mMainThreadStatements.GetCachedStatement(aQuery);
    }
    return mAsyncThreadStatements.GetCachedStatement(aQuery);
  }

  /**
   * Gets a cached asynchronous statement.
   *
   * @param aQuery
   *        SQL query literal.
   * @return The cached statement.
   * @note Always null check the result.
   * @note AsyncStatements are automatically reset on execution.
   */
  template<int N>
  already_AddRefed<mozIStorageAsyncStatement>
  GetAsyncStatement(const char (&aQuery)[N]) const
  {
    nsDependentCString query(aQuery, N - 1);
    return GetAsyncStatement(query);
  }

  /**
   * Gets a cached asynchronous statement.
   *
   * @param aQuery
   *        nsCString of SQL query.
   * @return The cached statement.
   * @note Always null check the result.
   * @note AsyncStatements are automatically reset on execution.
   */
  already_AddRefed<mozIStorageAsyncStatement>
  GetAsyncStatement(const nsACString& aQuery) const
  {
    if (mShuttingDown) {
      return nsnull;
    }
    MOZ_ASSERT(NS_IsMainThread());
    return mMainThreadAsyncStatements.GetCachedStatement(aQuery);
  }

protected:
  /**
   * Initializes the database file.  If the database does not exist or is
   * corrupt, a new one is created.  In case of corruption it also creates a
   * backup copy of the database.
   *
   * @param aStorage
   *        mozStorage service instance.
   * @param aNewDatabaseCreated
   *        whether a new database file has been created.
   */
  nsresult InitDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage,
                            bool* aNewDatabaseCreated);

  /**
   * Creates a database backup and replaces the original file with a new
   * one.
   *
   * @param aStorage
   *        mozStorage service instance.
   */
  nsresult BackupAndReplaceDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage);

  /**
   * Initializes the database.  This performs any necessary migrations for the
   * database.  All migration is done inside a transaction that is rolled back
   * if any error occurs.
   * @param aDatabaseMigrated
   *        Whether a schema upgrade happened.
   */
  nsresult InitSchema(bool* aDatabaseMigrated);

  /**
   * Initializes additionale SQLite functions, defined in SQLFunctions.h
   */
  nsresult InitFunctions();

  /**
   * Initializes triggers defined in nsPlacesTriggers.h
   */  
  nsresult InitTempTriggers();

  /**
   * Helpers used by schema upgrades.
   */
  nsresult MigrateV7Up();
  nsresult MigrateV8Up();
  nsresult MigrateV9Up();
  nsresult MigrateV10Up();
  nsresult MigrateV11Up();
  nsresult CheckAndUpdateGUIDs();

private:
  ~Database();

  /**
   * Singleton getter, invoked by class instantiation.
   *
   * Note: does AddRef.
   */
  static Database* GetSingleton();

  static Database* gDatabase;

  nsCOMPtr<mozIStorageConnection> mMainConn;

  mutable StatementCache mMainThreadStatements;
  mutable AsyncStatementCache mMainThreadAsyncStatements;
  mutable StatementCache mAsyncThreadStatements;

  PRInt32 mDBPageSize;
  enum JournalMode mCurrentJournalMode;
  PRUint16 mDatabaseStatus;
  bool mShuttingDown;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_Database_h_
