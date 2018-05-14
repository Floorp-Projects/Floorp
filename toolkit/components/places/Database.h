/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_Database_h_
#define mozilla_places_Database_h_

#include "MainThreadUtils.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserver.h"
#include "nsIAsyncShutdown.h"
#include "mozilla/storage.h"
#include "mozilla/storage/StatementCache.h"
#include "mozilla/Attributes.h"
#include "nsIEventTarget.h"
#include "Shutdown.h"
#include "nsCategoryCache.h"

// This is the schema version. Update it at any schema change and add a
// corresponding migrateVxx method below.
#define DATABASE_SCHEMA_VERSION 48

// Fired after Places inited.
#define TOPIC_PLACES_INIT_COMPLETE "places-init-complete"
// This topic is received when the profile is about to be lost.  Places does
// initial shutdown work and notifies TOPIC_PLACES_SHUTDOWN to all listeners.
// Any shutdown work that requires the Places APIs should happen here.
#define TOPIC_PROFILE_CHANGE_TEARDOWN "profile-change-teardown"
// Fired when Places is shutting down.  Any code should stop accessing Places
// APIs after this notification.  If you need to listen for Places shutdown
// you should only use this notification, next ones are intended only for
// internal Places use.
#define TOPIC_PLACES_SHUTDOWN "places-shutdown"
// Fired when the connection has gone, nothing will work from now on.
#define TOPIC_PLACES_CONNECTION_CLOSED "places-connection-closed"

// Simulate profile-before-change. This topic may only be used by
// calling `observe` directly on the database. Used for testing only.
#define TOPIC_SIMULATE_PLACES_SHUTDOWN "test-simulate-places-shutdown"

class nsIRunnable;

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

class ClientsShutdownBlocker;
class ConnectionShutdownBlocker;

class Database final : public nsIObserver
                     , public nsSupportsWeakReference
{
  typedef mozilla::storage::StatementCache<mozIStorageStatement> StatementCache;
  typedef mozilla::storage::StatementCache<mozIStorageAsyncStatement> AsyncStatementCache;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  Database();

  /**
   * Initializes the database connection and the schema.
   * In case of corruption the database is copied to a backup file and replaced.
   */
  nsresult Init();

  /**
   * The AsyncShutdown client used by clients of this API to be informed of shutdown.
   */
  already_AddRefed<nsIAsyncShutdownClient> GetClientsShutdown();

  /**
   * The AsyncShutdown client used by clients of this API to be informed of connection shutdown.
   */
  already_AddRefed<nsIAsyncShutdownClient> GetConnectionShutdown();

  /**
   * Getter to use when instantiating the class.
   *
   * @return Singleton instance of this class.
   */
  static already_AddRefed<Database> GetDatabase();

  /**
   * Actually initialized the connection on first need.
   */
  nsresult EnsureConnection();

  /**
   * Notifies that the connection has been initialized.
   */
  nsresult NotifyConnectionInitalized();

  /**
   * Returns last known database status.
   *
   * @return one of the nsINavHistoryService::DATABASE_STATUS_* constants.
   */
  uint16_t GetDatabaseStatus()
  {
    mozilla::Unused << EnsureConnection();
    return mDatabaseStatus;
  }

  /**
   * Returns a pointer to the storage connection.
   *
   * @return The connection handle.
   */
  mozIStorageConnection* MainConn()
  {
    mozilla::Unused << EnsureConnection();
    return mMainConn;
  }

  /**
   * Dispatches a runnable to the connection async thread, to be serialized
   * with async statements.
   *
   * @param aEvent
   *        The runnable to be dispatched.
   */
  void DispatchToAsyncThread(nsIRunnable* aEvent)
  {
    if (mClosed || NS_FAILED(EnsureConnection())) {
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
  GetStatement(const char (&aQuery)[N])
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
  already_AddRefed<mozIStorageStatement>  GetStatement(const nsACString& aQuery);

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
  GetAsyncStatement(const char (&aQuery)[N])
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
  already_AddRefed<mozIStorageAsyncStatement> GetAsyncStatement(const nsACString& aQuery);

  uint32_t MaxUrlLength();

  int64_t GetRootFolderId() {
    mozilla::Unused << EnsureConnection();
    return mRootId;
  }
  int64_t GetMenuFolderId() {
    mozilla::Unused << EnsureConnection();
    return mMenuRootId;
  }
  int64_t GetTagsFolderId() {
    mozilla::Unused << EnsureConnection();
    return mTagsRootId;
  }
  int64_t GetUnfiledFolderId() {
    mozilla::Unused << EnsureConnection();
    return mUnfiledRootId;
  }
  int64_t GetToolbarFolderId() {
    mozilla::Unused << EnsureConnection();
    return mToolbarRootId;
  }
  int64_t GetMobileFolderId() {
    mozilla::Unused << EnsureConnection();
    return mMobileRootId;
  }

protected:
  /**
   * Finalizes the cached statements and closes the database connection.
   * A TOPIC_PLACES_CONNECTION_CLOSED notification is fired when done.
   */
  void Shutdown();

  bool IsShutdownStarted() const;

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
   * Ensure the favicons database file exists.
   *
   * @param aStorage
   *        mozStorage service instance.
   */
  nsresult EnsureFaviconsDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage);

  /**
   * Creates a database backup and replaces the original file with a new
   * one.
   *
   * @param aStorage
   *        mozStorage service instance.
   * @param aTryToClone
   *        whether we should try to clone a corrupt database.
   */
  nsresult BackupAndReplaceDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage,
                                        bool aTryToClone);

  /**
   * Tries to recover tables and their contents from a corrupt database.
   *
   * @param aStorage
   *        mozStorage service instance.
   */
  nsresult TryToCloneTablesFromCorruptDatabase(nsCOMPtr<mozIStorageService>& aStorage);

  /**
   * Set up the connection environment through PRAGMAs.
   * Will return NS_ERROR_FILE_CORRUPTED if any critical setting fails.
   *
   * @param aStorage
   *        mozStorage service instance.
   */
  nsresult SetupDatabaseConnection(nsCOMPtr<mozIStorageService>& aStorage);

  /**
   * Initializes the schema.  This performs any necessary migrations for the
   * database.  All migration is done inside a transaction that is rolled back
   * if any error occurs.
   * @param aDatabaseMigrated
   *        Whether a schema upgrade happened.
   */
  nsresult InitSchema(bool* aDatabaseMigrated);

  /**
   * Checks the root bookmark folders are present, and saves the IDs for them.
   */
  nsresult CheckRoots();

  /**
   * Creates bookmark roots in a new DB.
   */
  nsresult EnsureBookmarkRoots(const int32_t startPosition);

  /**
   * Initializes additionale SQLite functions, defined in SQLFunctions.h
   */
  nsresult InitFunctions();

  /**
   * Initializes temp entities, like triggers, tables, views...
   */
  nsresult InitTempEntities();

  /**
   * Helpers used by schema upgrades.
   */
  nsresult MigrateV31Up();
  nsresult MigrateV32Up();
  nsresult MigrateV33Up();
  nsresult MigrateV34Up();
  nsresult MigrateV35Up();
  nsresult MigrateV36Up();
  nsresult MigrateV37Up();
  nsresult MigrateV38Up();
  nsresult MigrateV39Up();
  nsresult MigrateV40Up();
  nsresult MigrateV41Up();
  nsresult MigrateV42Up();
  nsresult MigrateV43Up();
  nsresult MigrateV44Up();
  nsresult MigrateV45Up();
  nsresult MigrateV46Up();
  nsresult MigrateV47Up();
  nsresult MigrateV48Up();

  void MigrateV48Frecencies();

  nsresult UpdateBookmarkRootTitles();

  friend class ConnectionShutdownBlocker;

  int64_t CreateMobileRoot();
  nsresult GetItemsWithAnno(const nsACString& aAnnoName, int32_t aItemType,
                            nsTArray<int64_t>& aItemIds);
  nsresult DeleteBookmarkItem(int32_t aItemId);

private:
  ~Database();

  /**
   * Singleton getter, invoked by class instantiation.
   */
  static already_AddRefed<Database> GetSingleton();

  static Database* gDatabase;

  nsCOMPtr<mozIStorageConnection> mMainConn;

  mutable StatementCache mMainThreadStatements;
  mutable AsyncStatementCache mMainThreadAsyncStatements;
  mutable StatementCache mAsyncThreadStatements;

  int32_t mDBPageSize;
  uint16_t mDatabaseStatus;
  bool mClosed;
  // Used to track whether icon payloads should be converted at the end of
  // schema migration.
  bool mShouldConvertIconPayloads;
  // Used to track whether the favicons database should be vacuumed at the end
  // of the schema migration.
  bool mShouldVacuumIcons;

  /**
   * Phases for shutting down the Database.
   * See Shutdown.h for further details about the shutdown procedure.
   */
  already_AddRefed<nsIAsyncShutdownClient> GetProfileChangeTeardownPhase();
  already_AddRefed<nsIAsyncShutdownClient> GetProfileBeforeChangePhase();

  /**
   * Blockers in charge of waiting for the Places clients and then shutting
   * down the mozStorage connection.
   * See Shutdown.h for further details about the shutdown procedure.
   *
   * Cycles with these are broken in `Shutdown()`.
   */
  RefPtr<ClientsShutdownBlocker> mClientsShutdown;
  RefPtr<ConnectionShutdownBlocker> mConnectionShutdown;

  // Maximum length of a stored url.
  // For performance reasons we don't store very long urls in history, since
  // they are slower to search through and cause abnormal database growth,
  // affecting the awesomebar fetch time.
  uint32_t mMaxUrlLength;

  // Used to initialize components on places startup.
  nsCategoryCache<nsIObserver> mCacheObservers;

  // Used to cache the places folder Ids when the connection is started.
  int64_t mRootId;
  int64_t mMenuRootId;
  int64_t mTagsRootId;
  int64_t mUnfiledRootId;
  int64_t mToolbarRootId;
  int64_t mMobileRootId;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_Database_h_
