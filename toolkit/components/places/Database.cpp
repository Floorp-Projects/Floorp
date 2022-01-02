/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/JSONWriter.h"

#include "Database.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIFile.h"

#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "nsPlacesTables.h"
#include "nsPlacesIndexes.h"
#include "nsPlacesTriggers.h"
#include "nsPlacesMacros.h"
#include "nsVariant.h"
#include "SQLFunctions.h"
#include "Helpers.h"
#include "nsFaviconService.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "prenv.h"
#include "prsystem.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozIStorageService.h"
#include "prtime.h"

#include "nsXULAppAPI.h"

// Time between corrupt database backups.
#define RECENT_BACKUP_TIME_MICROSEC (int64_t)86400 * PR_USEC_PER_SEC  // 24H

// Filename of the database.
#define DATABASE_FILENAME u"places.sqlite"_ns
// Filename of the icons database.
#define DATABASE_FAVICONS_FILENAME u"favicons.sqlite"_ns

// Set to the database file name when it was found corrupt by a previous
// maintenance run.
#define PREF_FORCE_DATABASE_REPLACEMENT \
  "places.database.replaceDatabaseOnStartup"

// Whether on corruption we should try to fix the database by cloning it.
#define PREF_DATABASE_CLONEONCORRUPTION "places.database.cloneOnCorruption"

// Set to specify the size of the places database growth increments in kibibytes
#define PREF_GROWTH_INCREMENT_KIB "places.database.growthIncrementKiB"

// Set to disable the default robust storage and use volatile, in-memory
// storage without robust transaction flushing guarantees. This makes
// SQLite use much less I/O at the cost of losing data when things crash.
// The pref is only honored if an environment variable is set. The env
// variable is intentionally named something scary to help prevent someone
// from thinking it is a useful performance optimization they should enable.
#define PREF_DISABLE_DURABILITY "places.database.disableDurability"
#define ENV_ALLOW_CORRUPTION \
  "ALLOW_PLACES_DATABASE_TO_LOSE_DATA_AND_BECOME_CORRUPT"

#define PREF_MIGRATE_V52_ORIGIN_FRECENCIES \
  "places.database.migrateV52OriginFrecencies"

// Maximum size for the WAL file.
// For performance reasons this should be as large as possible, so that more
// transactions can fit into it, and the checkpoint cost is paid less often.
// At the same time, since we use synchronous = NORMAL, an fsync happens only
// at checkpoint time, so we don't want the WAL to grow too much and risk to
// lose all the contained transactions on a crash.
#define DATABASE_MAX_WAL_BYTES 2048000

// Since exceeding the journal limit will cause a truncate, we allow a slightly
// larger limit than DATABASE_MAX_WAL_BYTES to reduce the number of truncates.
// This is the number of bytes the journal can grow over the maximum wal size
// before being truncated.
#define DATABASE_JOURNAL_OVERHEAD_BYTES 2048000

#define BYTES_PER_KIBIBYTE 1024

// How much time Sqlite can wait before returning a SQLITE_BUSY error.
#define DATABASE_BUSY_TIMEOUT_MS 100

// This annotation is no longer used & is obsolete, but here for migration.
#define LAST_USED_ANNO "bookmarkPropertiesDialog/folderLastUsed"_ns
// This is key in the meta table that the LAST_USED_ANNO is migrated to.
#define LAST_USED_FOLDERS_META_KEY "places/bookmarks/edit/lastusedfolder"_ns

// We use a fixed title for the mobile root to avoid marking the database as
// corrupt if we can't look up the localized title in the string bundle. Sync
// sets the title to the localized version when it creates the left pane query.
#define MOBILE_ROOT_TITLE "mobile"

// Legacy item annotation used by the old Sync engine.
#define SYNC_PARENT_ANNO "sync/parent"

using namespace mozilla;

namespace mozilla {
namespace places {

namespace {

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Get the filename for a corrupt database.
 */
nsString getCorruptFilename(const nsString& aDbFilename) {
  return aDbFilename + u".corrupt"_ns;
}
/**
 * Get the filename for a recover database.
 */
nsString getRecoverFilename(const nsString& aDbFilename) {
  return aDbFilename + u".recover"_ns;
}

/**
 * Checks whether exists a corrupt database file created not longer than
 * RECENT_BACKUP_TIME_MICROSEC ago.
 */
bool isRecentCorruptFile(const nsCOMPtr<nsIFile>& aCorruptFile) {
  MOZ_ASSERT(NS_IsMainThread());
  bool fileExists = false;
  if (NS_FAILED(aCorruptFile->Exists(&fileExists)) || !fileExists) {
    return false;
  }
  PRTime lastMod = 0;
  if (NS_FAILED(aCorruptFile->GetLastModifiedTime(&lastMod)) || lastMod <= 0 ||
      (PR_Now() - lastMod) > RECENT_BACKUP_TIME_MICROSEC) {
    return false;
  }
  return true;
}

/**
 * Sets the connection journal mode to one of the JOURNAL_* types.
 *
 * @param aDBConn
 *        The database connection.
 * @param aJournalMode
 *        One of the JOURNAL_* types.
 * @returns the current journal mode.
 * @note this may return a different journal mode than the required one, since
 *       setting it may fail.
 */
enum JournalMode SetJournalMode(nsCOMPtr<mozIStorageConnection>& aDBConn,
                                enum JournalMode aJournalMode) {
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString journalMode;
  switch (aJournalMode) {
    default:
      MOZ_FALLTHROUGH_ASSERT("Trying to set an unknown journal mode.");
      // Fall through to the default DELETE journal.
    case JOURNAL_DELETE:
      journalMode.AssignLiteral("delete");
      break;
    case JOURNAL_TRUNCATE:
      journalMode.AssignLiteral("truncate");
      break;
    case JOURNAL_MEMORY:
      journalMode.AssignLiteral("memory");
      break;
    case JOURNAL_WAL:
      journalMode.AssignLiteral("wal");
      break;
  }

  nsCOMPtr<mozIStorageStatement> statement;
  nsAutoCString query(MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA journal_mode = ");
  query.Append(journalMode);
  aDBConn->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_TRUE(statement, JOURNAL_DELETE);

  bool hasResult = false;
  if (NS_SUCCEEDED(statement->ExecuteStep(&hasResult)) && hasResult &&
      NS_SUCCEEDED(statement->GetUTF8String(0, journalMode))) {
    if (journalMode.EqualsLiteral("delete")) {
      return JOURNAL_DELETE;
    }
    if (journalMode.EqualsLiteral("truncate")) {
      return JOURNAL_TRUNCATE;
    }
    if (journalMode.EqualsLiteral("memory")) {
      return JOURNAL_MEMORY;
    }
    if (journalMode.EqualsLiteral("wal")) {
      return JOURNAL_WAL;
    }
    MOZ_ASSERT(false, "Got an unknown journal mode.");
  }

  return JOURNAL_DELETE;
}

nsresult CreateRoot(nsCOMPtr<mozIStorageConnection>& aDBConn,
                    const nsCString& aRootName, const nsCString& aGuid,
                    const nsCString& titleString, const int32_t position,
                    int64_t& newId) {
  MOZ_ASSERT(NS_IsMainThread());

  // A single creation timestamp for all roots so that the root folder's
  // last modification time isn't earlier than its childrens' creation time.
  static PRTime timestamp = 0;
  if (!timestamp) timestamp = RoundedPRNow();

  // Create a new bookmark folder for the root.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDBConn->CreateStatement(
      nsLiteralCString(
          "INSERT INTO moz_bookmarks "
          "(type, position, title, dateAdded, lastModified, guid, parent, "
          "syncChangeCounter, syncStatus) "
          "VALUES (:item_type, :item_position, :item_title,"
          ":date_added, :last_modified, :guid, "
          "IFNULL((SELECT id FROM moz_bookmarks WHERE parent = 0), 0), "
          "1, :sync_status)"),
      getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  rv = stmt->BindInt32ByName("item_type"_ns,
                             nsINavBookmarksService::TYPE_FOLDER);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt32ByName("item_position"_ns, position);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindUTF8StringByName("item_title"_ns, titleString);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt64ByName("date_added"_ns, timestamp);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt64ByName("last_modified"_ns, timestamp);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindUTF8StringByName("guid"_ns, aGuid);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt32ByName("sync_status"_ns,
                             nsINavBookmarksService::SYNC_STATUS_NEW);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->Execute();
  if (NS_FAILED(rv)) return rv;

  newId = nsNavBookmarks::sLastInsertedItemId;
  return NS_OK;
}

nsresult SetupDurability(nsCOMPtr<mozIStorageConnection>& aDBConn,
                         int32_t aDBPageSize) {
  nsresult rv;
  if (PR_GetEnv(ENV_ALLOW_CORRUPTION) &&
      Preferences::GetBool(PREF_DISABLE_DURABILITY, false)) {
    // Volatile storage was requested. Use the in-memory journal (no
    // filesystem I/O) and don't sync the filesystem after writing.
    SetJournalMode(aDBConn, JOURNAL_MEMORY);
    rv = aDBConn->ExecuteSimpleSQL("PRAGMA synchronous = OFF"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Be sure to set journal mode after page_size.  WAL would prevent the
    // change otherwise.
    if (JOURNAL_WAL == SetJournalMode(aDBConn, JOURNAL_WAL)) {
      // Set the WAL journal size limit.
      int32_t checkpointPages =
          static_cast<int32_t>(DATABASE_MAX_WAL_BYTES / aDBPageSize);
      nsAutoCString checkpointPragma("PRAGMA wal_autocheckpoint = ");
      checkpointPragma.AppendInt(checkpointPages);
      rv = aDBConn->ExecuteSimpleSQL(checkpointPragma);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // Ignore errors, if we fail here the database could be considered corrupt
      // and we won't be able to go on, even if it's just matter of a bogus file
      // system.  The default mode (DELETE) will be fine in such a case.
      (void)SetJournalMode(aDBConn, JOURNAL_TRUNCATE);

      // Set synchronous to FULL to ensure maximum data integrity, even in
      // case of crashes or unclean shutdowns.
      rv = aDBConn->ExecuteSimpleSQL("PRAGMA synchronous = FULL"_ns);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // The journal is usually free to grow for performance reasons, but it never
  // shrinks back.  Since the space taken may be problematic, limit its size.
  nsAutoCString journalSizePragma("PRAGMA journal_size_limit = ");
  journalSizePragma.AppendInt(DATABASE_MAX_WAL_BYTES +
                              DATABASE_JOURNAL_OVERHEAD_BYTES);
  (void)aDBConn->ExecuteSimpleSQL(journalSizePragma);

  // Grow places in |growthIncrementKiB| increments to limit fragmentation on
  // disk. By default, it's 5 MB.
  int32_t growthIncrementKiB =
      Preferences::GetInt(PREF_GROWTH_INCREMENT_KIB, 5 * BYTES_PER_KIBIBYTE);
  if (growthIncrementKiB > 0) {
    (void)aDBConn->SetGrowthIncrement(growthIncrementKiB * BYTES_PER_KIBIBYTE,
                                      ""_ns);
  }
  return NS_OK;
}

nsresult AttachDatabase(nsCOMPtr<mozIStorageConnection>& aDBConn,
                        const nsACString& aPath, const nsACString& aName) {
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDBConn->CreateStatement("ATTACH DATABASE :path AS "_ns + aName,
                                         getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName("path"_ns, aPath);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // The journal limit must be set apart for each database.
  nsAutoCString journalSizePragma("PRAGMA favicons.journal_size_limit = ");
  journalSizePragma.AppendInt(DATABASE_MAX_WAL_BYTES +
                              DATABASE_JOURNAL_OVERHEAD_BYTES);
  Unused << aDBConn->ExecuteSimpleSQL(journalSizePragma);

  return NS_OK;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//// Database

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(Database, gDatabase)

NS_IMPL_ISUPPORTS(Database, nsIObserver, nsISupportsWeakReference)

Database::Database()
    : mMainThreadStatements(mMainConn),
      mMainThreadAsyncStatements(mMainConn),
      mAsyncThreadStatements(mMainConn),
      mDBPageSize(0),
      mDatabaseStatus(nsINavHistoryService::DATABASE_STATUS_OK),
      mClosed(false),
      mClientsShutdown(new ClientsShutdownBlocker()),
      mConnectionShutdown(new ConnectionShutdownBlocker(this)),
      mMaxUrlLength(0),
      mCacheObservers(TOPIC_PLACES_INIT_COMPLETE),
      mRootId(-1),
      mMenuRootId(-1),
      mTagsRootId(-1),
      mUnfiledRootId(-1),
      mToolbarRootId(-1),
      mMobileRootId(-1) {
  MOZ_ASSERT(!XRE_IsContentProcess(),
             "Cannot instantiate Places in the content process");
  // Attempting to create two instances of the service?
  MOZ_ASSERT(!gDatabase);
  gDatabase = this;
}

already_AddRefed<nsIAsyncShutdownClient>
Database::GetProfileChangeTeardownPhase() {
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc =
      services::GetAsyncShutdownService();
  MOZ_ASSERT(asyncShutdownSvc);
  if (NS_WARN_IF(!asyncShutdownSvc)) {
    return nullptr;
  }

  // Consumers of Places should shutdown before us, at profile-change-teardown.
  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  DebugOnly<nsresult> rv =
      asyncShutdownSvc->GetProfileChangeTeardown(getter_AddRefs(shutdownPhase));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}

already_AddRefed<nsIAsyncShutdownClient>
Database::GetProfileBeforeChangePhase() {
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc =
      services::GetAsyncShutdownService();
  MOZ_ASSERT(asyncShutdownSvc);
  if (NS_WARN_IF(!asyncShutdownSvc)) {
    return nullptr;
  }

  // Consumers of Places should shutdown before us, at profile-change-teardown.
  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  DebugOnly<nsresult> rv =
      asyncShutdownSvc->GetProfileBeforeChange(getter_AddRefs(shutdownPhase));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}

Database::~Database() = default;

already_AddRefed<mozIStorageAsyncStatement> Database::GetAsyncStatement(
    const nsACString& aQuery) {
  if (PlacesShutdownBlocker::sIsStarted || NS_FAILED(EnsureConnection())) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());
  return mMainThreadAsyncStatements.GetCachedStatement(aQuery);
}

already_AddRefed<mozIStorageStatement> Database::GetStatement(
    const nsACString& aQuery) {
  if (PlacesShutdownBlocker::sIsStarted) {
    return nullptr;
  }
  if (NS_IsMainThread()) {
    if (NS_FAILED(EnsureConnection())) {
      return nullptr;
    }
    return mMainThreadStatements.GetCachedStatement(aQuery);
  }
  // In the async case, the connection must have been started on the main-thread
  // already.
  MOZ_ASSERT(mMainConn);
  return mAsyncThreadStatements.GetCachedStatement(aQuery);
}

already_AddRefed<nsIAsyncShutdownClient> Database::GetClientsShutdown() {
  if (mClientsShutdown) return mClientsShutdown->GetClient();
  return nullptr;
}

already_AddRefed<nsIAsyncShutdownClient> Database::GetConnectionShutdown() {
  if (mConnectionShutdown) return mConnectionShutdown->GetClient();
  return nullptr;
}

// static
already_AddRefed<Database> Database::GetDatabase() {
  if (PlacesShutdownBlocker::sIsStarted) {
    return nullptr;
  }
  return GetSingleton();
}

nsresult Database::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  // DO NOT FAIL HERE, otherwise we would never break the cycle between this
  // object and the shutdown blockers, causing unexpected leaks.

  {
    // First of all Places clients should block profile-change-teardown.
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
        GetProfileChangeTeardownPhase();
    MOZ_ASSERT(shutdownPhase);
    if (shutdownPhase) {
      DebugOnly<nsresult> rv = shutdownPhase->AddBlocker(
          static_cast<nsIAsyncShutdownBlocker*>(mClientsShutdown.get()),
          NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, u""_ns);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  {
    // Then connection closing should block profile-before-change.
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
        GetProfileBeforeChangePhase();
    MOZ_ASSERT(shutdownPhase);
    if (shutdownPhase) {
      DebugOnly<nsresult> rv = shutdownPhase->AddBlocker(
          static_cast<nsIAsyncShutdownBlocker*>(mConnectionShutdown.get()),
          NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, u""_ns);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  // Finally observe profile shutdown notifications.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->AddObserver(this, TOPIC_PROFILE_CHANGE_TEARDOWN, true);
  }
  return NS_OK;
}

nsresult Database::EnsureConnection() {
  // Run this only once.
  if (mMainConn ||
      mDatabaseStatus == nsINavHistoryService::DATABASE_STATUS_LOCKED) {
    return NS_OK;
  }
  // Don't try to create a database too late.
  if (PlacesShutdownBlocker::sIsStarted) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(NS_IsMainThread(),
             "Database initialization must happen on the main-thread");

  {
    bool initSucceeded = false;
    auto notify = MakeScopeExit([&]() {
      // If the database connection cannot be opened, it may just be locked
      // by third parties.  Set a locked state.
      if (!initSucceeded) {
        mMainConn = nullptr;
        mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_LOCKED;
      }
      // Notify at the next tick, to avoid re-entrancy problems.
      NS_DispatchToMainThread(
          NewRunnableMethod("places::Database::EnsureConnection()", this,
                            &Database::NotifyConnectionInitalized));
    });

    nsCOMPtr<mozIStorageService> storage =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    NS_ENSURE_STATE(storage);

    nsCOMPtr<nsIFile> profileDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(profileDir));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> databaseFile;
    rv = profileDir->Clone(getter_AddRefs(databaseFile));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = databaseFile->Append(DATABASE_FILENAME);
    NS_ENSURE_SUCCESS(rv, rv);
    bool databaseExisted = false;
    rv = databaseFile->Exists(&databaseExisted);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString corruptDbName;
    if (NS_SUCCEEDED(Preferences::GetString(PREF_FORCE_DATABASE_REPLACEMENT,
                                            corruptDbName)) &&
        !corruptDbName.IsEmpty()) {
      // If this pref is set, maintenance required a database replacement, due
      // to integrity corruption. Be sure to clear the pref to avoid handling it
      // more than once.
      (void)Preferences::ClearUser(PREF_FORCE_DATABASE_REPLACEMENT);

      // The database is corrupt, backup and replace it with a new one.
      nsCOMPtr<nsIFile> fileToBeReplaced;
      bool fileExists = false;
      if (NS_SUCCEEDED(profileDir->Clone(getter_AddRefs(fileToBeReplaced))) &&
          NS_SUCCEEDED(fileToBeReplaced->Exists(&fileExists)) && fileExists) {
        rv = BackupAndReplaceDatabaseFile(storage, corruptDbName, true, false);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // Open the database file.  If it does not exist a new one will be created.
    // Use an unshared connection, it will consume more memory but avoid shared
    // cache contentions across threads.
    rv = storage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(mMainConn));
    if (NS_SUCCEEDED(rv) && !databaseExisted) {
      mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CREATE;
    } else if (rv == NS_ERROR_FILE_CORRUPTED) {
      // The database is corrupt, backup and replace it with a new one.
      rv = BackupAndReplaceDatabaseFile(storage, DATABASE_FILENAME, true, true);
      // Fallback to catch-all handler.
    }
    NS_ENSURE_SUCCESS(rv, rv);

    // Initialize the database schema.  In case of failure the existing schema
    // is is corrupt or incoherent, thus the database should be replaced.
    bool databaseMigrated = false;
    rv = SetupDatabaseConnection(storage);
    bool shouldTryToCloneDb = true;
    if (NS_SUCCEEDED(rv)) {
      // Failing to initialize the schema may indicate a corruption.
      rv = InitSchema(&databaseMigrated);
      if (NS_FAILED(rv)) {
        // Cloning the db on a schema migration may not be a good idea, since we
        // may end up cloning the schema problems.
        shouldTryToCloneDb = false;
        if (rv == NS_ERROR_STORAGE_BUSY || rv == NS_ERROR_FILE_IS_LOCKED ||
            rv == NS_ERROR_FILE_NO_DEVICE_SPACE ||
            rv == NS_ERROR_OUT_OF_MEMORY) {
          // The database is not corrupt, though some migration step failed.
          // This may be caused by concurrent use of sync and async Storage APIs
          // or by a system issue.
          // The best we can do is trying again. If it should still fail, Places
          // won't work properly and will be handled as LOCKED.
          rv = InitSchema(&databaseMigrated);
          if (NS_FAILED(rv)) {
            rv = NS_ERROR_FILE_IS_LOCKED;
          }
        } else {
          rv = NS_ERROR_FILE_CORRUPTED;
        }
      }
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (rv != NS_ERROR_FILE_IS_LOCKED) {
        mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CORRUPT;
      }
      // Some errors may not indicate a database corruption, for those cases we
      // just bail out without throwing away a possibly valid places.sqlite.
      if (rv == NS_ERROR_FILE_CORRUPTED) {
        // Since we don't know which database is corrupt, we must replace both.
        rv = BackupAndReplaceDatabaseFile(storage, DATABASE_FAVICONS_FILENAME,
                                          false, false);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = BackupAndReplaceDatabaseFile(storage, DATABASE_FILENAME,
                                          shouldTryToCloneDb, true);
        NS_ENSURE_SUCCESS(rv, rv);
        // Try to initialize the new database again.
        rv = SetupDatabaseConnection(storage);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = InitSchema(&databaseMigrated);
      }
      // Bail out if we couldn't fix the database.
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (databaseMigrated) {
      mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_UPGRADED;
    }

    // Initialize here all the items that are not part of the on-disk database,
    // like views, temp triggers or temp tables.  The database should not be
    // considered corrupt if any of the following fails.

    rv = InitTempEntities();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CheckRoots();
    NS_ENSURE_SUCCESS(rv, rv);

    initSucceeded = true;
  }
  return NS_OK;
}

nsresult Database::NotifyConnectionInitalized() {
  MOZ_ASSERT(NS_IsMainThread());
  // Notify about Places initialization.
  nsCOMArray<nsIObserver> entries;
  mCacheObservers.GetEntries(entries);
  for (int32_t idx = 0; idx < entries.Count(); ++idx) {
    MOZ_ALWAYS_SUCCEEDS(
        entries[idx]->Observe(nullptr, TOPIC_PLACES_INIT_COMPLETE, nullptr));
  }
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    MOZ_ALWAYS_SUCCEEDS(
        obs->NotifyObservers(nullptr, TOPIC_PLACES_INIT_COMPLETE, nullptr));
  }
  return NS_OK;
}

nsresult Database::EnsureFaviconsDatabaseAttached(
    const nsCOMPtr<mozIStorageService>& aStorage) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> databaseFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                         getter_AddRefs(databaseFile));
  NS_ENSURE_STATE(databaseFile);
  nsresult rv = databaseFile->Append(DATABASE_FAVICONS_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);
  nsString iconsPath;
  rv = databaseFile->GetPath(iconsPath);
  NS_ENSURE_SUCCESS(rv, rv);

  bool fileExists = false;
  if (NS_SUCCEEDED(databaseFile->Exists(&fileExists)) && fileExists) {
    return AttachDatabase(mMainConn, NS_ConvertUTF16toUTF8(iconsPath),
                          "favicons"_ns);
  }

  // Open the database file, this will also create it.
  nsCOMPtr<mozIStorageConnection> conn;
  rv = aStorage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(conn));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    // Ensure we'll close the connection when done.
    auto cleanup = MakeScopeExit([&]() {
      // We cannot use AsyncClose() here, because by the time we try to ATTACH
      // this database, its transaction could be still be running and that would
      // cause the ATTACH query to fail.
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(conn->Close()));
    });

    // Enable incremental vacuum for this database. Since it will contain even
    // large blobs and can be cleared with history, it's worth to have it.
    // Note that it will be necessary to manually use PRAGMA incremental_vacuum.
    rv = conn->ExecuteSimpleSQL("PRAGMA auto_vacuum = INCREMENTAL"_ns);
    NS_ENSURE_SUCCESS(rv, rv);

#if !defined(HAVE_64BIT_BUILD)
    // Ensure that temp tables are held in memory, not on disk, on 32 bit
    // platforms.
    rv = conn->ExecuteSimpleSQL("PRAGMA temp_store = MEMORY"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
#endif

    int32_t defaultPageSize;
    rv = conn->GetDefaultPageSize(&defaultPageSize);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetupDurability(conn, defaultPageSize);
    NS_ENSURE_SUCCESS(rv, rv);

    // We are going to update the database, so everything from now on should be
    // in a transaction for performances.
    mozStorageTransaction transaction(conn, false);
    // XXX Handle the error, bug 1696133.
    Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));
    rv = conn->ExecuteSimpleSQL(CREATE_MOZ_ICONS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ICONS_ICONURLHASH);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(CREATE_MOZ_PAGES_W_ICONS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PAGES_W_ICONS_ICONURLHASH);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(CREATE_MOZ_ICONS_TO_PAGES);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    // The scope exit will take care of closing the connection.
  }

  rv = AttachDatabase(mMainConn, NS_ConvertUTF16toUTF8(iconsPath),
                      "favicons"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::BackupAndReplaceDatabaseFile(
    nsCOMPtr<mozIStorageService>& aStorage, const nsString& aDbFilename,
    bool aTryToClone, bool aReopenConnection) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aDbFilename.Equals(DATABASE_FILENAME)) {
    mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CORRUPT;
  } else {
    // Due to OS file lockings, attached databases can't be cloned properly,
    // otherwise trying to reattach them later would fail.
    aTryToClone = false;
  }

  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIFile> databaseFile;
  rv = profDir->Clone(getter_AddRefs(databaseFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = databaseFile->Append(aDbFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we already failed in the last 24 hours avoid to create another corrupt
  // file, since doing so, in some situation, could cause us to create a new
  // corrupt file at every try to access any Places service.  That is bad
  // because it would quickly fill the user's disk space without any notice.
  nsCOMPtr<nsIFile> corruptFile;
  rv = profDir->Clone(getter_AddRefs(corruptFile));
  NS_ENSURE_SUCCESS(rv, rv);
  nsString corruptFilename = getCorruptFilename(aDbFilename);
  rv = corruptFile->Append(corruptFilename);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isRecentCorruptFile(corruptFile)) {
    // Ensure we never create more than one corrupt file.
    nsCOMPtr<nsIFile> corruptFile;
    rv = profDir->Clone(getter_AddRefs(corruptFile));
    NS_ENSURE_SUCCESS(rv, rv);
    nsString corruptFilename = getCorruptFilename(aDbFilename);
    rv = corruptFile->Append(corruptFilename);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = corruptFile->Remove(false);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
        rv != NS_ERROR_FILE_NOT_FOUND) {
      return rv;
    }

    nsCOMPtr<nsIFile> backup;
    Unused << aStorage->BackupDatabaseFile(databaseFile, corruptFilename,
                                           profDir, getter_AddRefs(backup));
  }

  // If anything fails from this point on, we have a stale connection or
  // database file, and there's not much more we can do.
  // The only thing we can try to do is to replace the database on the next
  // startup, and report the problem through telemetry.
  {
    enum eCorruptDBReplaceStage : int8_t {
      stage_closing = 0,
      stage_removing,
      stage_reopening,
      stage_replaced,
      stage_cloning,
      stage_cloned
    };
    eCorruptDBReplaceStage stage = stage_closing;
    auto guard = MakeScopeExit([&]() {
      if (stage != stage_replaced) {
        // Reaching this point means the database is corrupt and we failed to
        // replace it.  For this session part of the application related to
        // bookmarks and history will misbehave.  The frontend may show a
        // "locked" notification to the user though.
        // Set up a pref to try replacing the database at the next startup.
        Preferences::SetString(PREF_FORCE_DATABASE_REPLACEMENT, aDbFilename);
      }
      // Report the corruption through telemetry.
      Telemetry::Accumulate(
          Telemetry::PLACES_DATABASE_CORRUPTION_HANDLING_STAGE,
          static_cast<int8_t>(stage));
    });

    // Close database connection if open.
    if (mMainConn) {
      rv = mMainConn->SpinningSynchronousClose();
      NS_ENSURE_SUCCESS(rv, rv);
      mMainConn = nullptr;
    }

    // Remove the broken database.
    stage = stage_removing;
    rv = databaseFile->Remove(false);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
        rv != NS_ERROR_FILE_NOT_FOUND) {
      return rv;
    }

    // Create a new database file and try to clone tables from the corrupt one.
    bool cloned = false;
    if (aTryToClone &&
        Preferences::GetBool(PREF_DATABASE_CLONEONCORRUPTION, true)) {
      stage = stage_cloning;
      rv = TryToCloneTablesFromCorruptDatabase(aStorage, databaseFile);
      if (NS_SUCCEEDED(rv)) {
        // If we cloned successfully, we should not consider the database
        // corrupt anymore, otherwise we could reimport default bookmarks.
        mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_OK;
        cloned = true;
      }
    }

    if (aReopenConnection) {
      // Use an unshared connection, it will consume more memory but avoid
      // shared cache contentions across threads.
      stage = stage_reopening;
      rv = aStorage->OpenUnsharedDatabase(databaseFile,
                                          getter_AddRefs(mMainConn));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    stage = cloned ? stage_cloned : stage_replaced;
  }

  return NS_OK;
}

nsresult Database::TryToCloneTablesFromCorruptDatabase(
    const nsCOMPtr<mozIStorageService>& aStorage,
    const nsCOMPtr<nsIFile>& aDatabaseFile) {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString filename;
  nsresult rv = aDatabaseFile->GetLeafName(filename);

  nsCOMPtr<nsIFile> corruptFile;
  rv = aDatabaseFile->Clone(getter_AddRefs(corruptFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = corruptFile->SetLeafName(getCorruptFilename(filename));
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString path;
  rv = corruptFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> recoverFile;
  rv = aDatabaseFile->Clone(getter_AddRefs(recoverFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = recoverFile->SetLeafName(getRecoverFilename(filename));
  NS_ENSURE_SUCCESS(rv, rv);
  // Ensure there's no previous recover file.
  rv = recoverFile->Remove(false);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> conn;
  auto guard = MakeScopeExit([&]() {
    if (conn) {
      Unused << conn->Close();
    }
    Unused << recoverFile->Remove(false);
  });

  rv = aStorage->OpenUnsharedDatabase(recoverFile, getter_AddRefs(conn));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = AttachDatabase(conn, NS_ConvertUTF16toUTF8(path), "corrupt"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(conn, false);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  // Copy the schema version.
  nsCOMPtr<mozIStorageStatement> stmt;
  (void)conn->CreateStatement("PRAGMA corrupt.user_version"_ns,
                              getter_AddRefs(stmt));
  NS_ENSURE_TRUE(stmt, NS_ERROR_OUT_OF_MEMORY);
  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t schemaVersion = stmt->AsInt32(0);
  rv = conn->SetSchemaVersion(schemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Recreate the tables.
  rv = conn->CreateStatement(
      nsLiteralCString(
          "SELECT name, sql FROM corrupt.sqlite_master "
          "WHERE type = 'table' AND name BETWEEN 'moz_' AND 'moza'"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsAutoCString name;
    rv = stmt->GetUTF8String(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString query;
    rv = stmt->GetUTF8String(1, query);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(query);
    NS_ENSURE_SUCCESS(rv, rv);
    // Copy the table contents.
    rv = conn->ExecuteSimpleSQL("INSERT INTO main."_ns + name +
                                " SELECT * FROM corrupt."_ns + name);
    if (NS_FAILED(rv)) {
      rv = conn->ExecuteSimpleSQL("INSERT INTO main."_ns + name +
                                  " SELECT * FROM corrupt."_ns + name +
                                  " ORDER BY rowid DESC"_ns);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Recreate the indices.  Doing this after data addition is faster.
  rv = conn->CreateStatement(
      nsLiteralCString(
          "SELECT sql FROM corrupt.sqlite_master "
          "WHERE type <> 'table' AND name BETWEEN 'moz_' AND 'moza'"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  hasResult = false;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsAutoCString query;
    rv = stmt->GetUTF8String(0, query);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = conn->ExecuteSimpleSQL(query);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->Finalize();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << conn->Close();
  conn = nullptr;
  rv = recoverFile->RenameTo(nullptr, filename);
  NS_ENSURE_SUCCESS(rv, rv);
  Unused << corruptFile->Remove(false);

  guard.release();
  return NS_OK;
}

nsresult Database::SetupDatabaseConnection(
    nsCOMPtr<mozIStorageService>& aStorage) {
  MOZ_ASSERT(NS_IsMainThread());

  // Using immediate transactions allows the main connection to retry writes
  // that fail with `SQLITE_BUSY` because a cloned connection has locked the
  // database for writing.
  nsresult rv = mMainConn->SetDefaultTransactionType(
      mozIStorageConnection::TRANSACTION_IMMEDIATE);
  NS_ENSURE_SUCCESS(rv, rv);

  // WARNING: any statement executed before setting the journal mode must be
  // finalized, since SQLite doesn't allow changing the journal mode if there
  // is any outstanding statement.

  {
    // Get the page size.  This may be different than the default if the
    // database file already existed with a different page size.
    nsCOMPtr<mozIStorageStatement> statement;
    rv = mMainConn->CreateStatement(
        nsLiteralCString(MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA page_size"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    bool hasResult = false;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FILE_CORRUPTED);
    rv = statement->GetInt32(0, &mDBPageSize);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && mDBPageSize > 0,
                   NS_ERROR_FILE_CORRUPTED);
  }

#if !defined(HAVE_64BIT_BUILD)
  // Ensure that temp tables are held in memory, not on disk, on 32 bit
  // platforms.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA temp_store = MEMORY"));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  rv = SetupDurability(mMainConn, mDBPageSize);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString busyTimeoutPragma("PRAGMA busy_timeout = ");
  busyTimeoutPragma.AppendInt(DATABASE_BUSY_TIMEOUT_MS);
  (void)mMainConn->ExecuteSimpleSQL(busyTimeoutPragma);

  // Enable FOREIGN KEY support. This is a strict requirement.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA foreign_keys = ON"));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FILE_CORRUPTED);
#ifdef DEBUG
  {
    // There are a few cases where setting foreign_keys doesn't work:
    //  * in the middle of a multi-statement transaction
    //  * if the SQLite library in use doesn't support them
    // Since we need foreign_keys, let's at least assert in debug mode.
    nsCOMPtr<mozIStorageStatement> stmt;
    mMainConn->CreateStatement("PRAGMA foreign_keys"_ns, getter_AddRefs(stmt));
    bool hasResult = false;
    if (stmt && NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
      int32_t fkState = stmt->AsInt32(0);
      MOZ_ASSERT(fkState, "Foreign keys should be enabled");
    }
  }
#endif

  // Attach the favicons database to the main connection.
  rv = EnsureFaviconsDatabaseAttached(aStorage);
  if (NS_FAILED(rv)) {
    // The favicons database may be corrupt. Try to replace and reattach it.
    nsCOMPtr<nsIFile> iconsFile;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(iconsFile));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = iconsFile->Append(DATABASE_FAVICONS_FILENAME);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = iconsFile->Remove(false);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
        rv != NS_ERROR_FILE_NOT_FOUND) {
      return rv;
    }
    rv = EnsureFaviconsDatabaseAttached(aStorage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create favicons temp entities.
  rv = mMainConn->ExecuteSimpleSQL(CREATE_ICONS_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  // We use our functions during migration, so initialize them now.
  rv = InitFunctions();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::InitSchema(bool* aDatabaseMigrated) {
  MOZ_ASSERT(NS_IsMainThread());
  *aDatabaseMigrated = false;

  // Get the database schema version.
  int32_t currentSchemaVersion;
  nsresult rv = mMainConn->GetSchemaVersion(&currentSchemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);
  bool databaseInitialized = currentSchemaVersion > 0;

  if (databaseInitialized && currentSchemaVersion == DATABASE_SCHEMA_VERSION) {
    // The database is up to date and ready to go.
    return NS_OK;
  }

  auto guard = MakeScopeExit([&]() {
    // These run at the end of the migration, out of the transaction,
    // regardless of its success.
    MigrateV52OriginFrecencies();
  });

  // We are going to update the database, so everything from now on should be in
  // a transaction for performances.
  mozStorageTransaction transaction(mMainConn, false);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  if (databaseInitialized) {
    // Migration How-to:
    //
    // 1. increment PLACES_SCHEMA_VERSION.
    // 2. implement a method that performs upgrade to your version from the
    //    previous one.
    //
    // NOTE: The downgrade process is pretty much complicated by the fact old
    //       versions cannot know what a new version is going to implement.
    //       The only thing we will do for downgrades is setting back the schema
    //       version, so that next upgrades will run again the migration step.

    if (currentSchemaVersion < DATABASE_SCHEMA_VERSION) {
      *aDatabaseMigrated = true;

      if (currentSchemaVersion < 43) {
        // These are versions older than Firefox 60 ESR that are not supported
        // anymore.  In this case it's safer to just replace the database.
        return NS_ERROR_FILE_CORRUPTED;
      }

      // Firefox 60 uses schema version 43. - This is an ESR.

      if (currentSchemaVersion < 44) {
        rv = MigrateV44Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 45) {
        rv = MigrateV45Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 46) {
        rv = MigrateV46Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 47) {
        rv = MigrateV47Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 61 uses schema version 47.

      if (currentSchemaVersion < 48) {
        rv = MigrateV48Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 49) {
        rv = MigrateV49Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 50) {
        rv = MigrateV50Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 51) {
        rv = MigrateV51Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 52) {
        rv = MigrateV52Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 62 uses schema version 52.
      // Firefox 68 uses schema version 52. - This is an ESR.

      if (currentSchemaVersion < 53) {
        rv = MigrateV53Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 69 uses schema version 53
      // Firefox 78 uses schema version 53 - This is an ESR.

      if (currentSchemaVersion < 54) {
        rv = MigrateV54Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 81 uses schema version 54

      if (currentSchemaVersion < 55) {
        rv = MigrateV55Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 56) {
        rv = MigrateV56Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 57) {
        rv = MigrateV57Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 91 uses schema version 57

      if (currentSchemaVersion < 58) {
        rv = MigrateV58Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 92 uses schema version 58

      if (currentSchemaVersion < 59) {
        rv = MigrateV59Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 94 uses schema version 59

      if (currentSchemaVersion < 60) {
        rv = MigrateV60Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 96 uses schema version 60

      // Schema Upgrades must add migration code here.
      // >>> IMPORTANT! <<<
      // NEVER MIX UP SYNC AND ASYNC EXECUTION IN MIGRATORS, YOU MAY LOCK THE
      // CONNECTION AND CAUSE FURTHER STEPS TO FAIL.
      // In case, set a bool and do the async work in the ScopeExit guard just
      // before the migration steps.
    }
  } else {
    // This is a new database, so we have to create all the tables and indices.

    // moz_origins.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_ORIGINS);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_URL_HASH);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_REVHOST);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_VISITCOUNT);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FRECENCY);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_LASTVISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_ORIGIN_ID);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_historyvisits.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_FROMVISIT);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_VISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_inputhistory.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_INPUTHISTORY);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_bookmarks.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS_DELETED);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACETYPE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PARENTPOSITION);
    NS_ENSURE_SUCCESS(rv, rv);
    rv =
        mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_DATEADDED);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_keywords.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_KEYWORDS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_KEYWORDS_PLACEPOSTDATA);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_anno_attributes.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_ANNO_ATTRIBUTES);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_annos.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_ANNOS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_items_annos.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_ITEMS_ANNOS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ITEMSANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_meta.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_META);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places_metadata
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(
        CREATE_IDX_MOZ_PLACES_METADATA_PLACECREATED);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places_metadata_search_queries
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SEARCH_QUERIES);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places_metadata_snapshots
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SNAPSHOTS);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places_metadata_snapshots_extra
    rv =
        mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SNAPSHOTS_EXTRA);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_places_metadata_snapshots_groups
    rv = mMainConn->ExecuteSimpleSQL(
        CREATE_MOZ_PLACES_METADATA_SNAPSHOTS_GROUPS);
    NS_ENSURE_SUCCESS(rv, rv);
    // moz_places_metadata_groups_to_snapshots
    rv = mMainConn->ExecuteSimpleSQL(
        CREATE_MOZ_PLACES_METADATA_GROUPS_TO_SNAPSHOTS);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_session_metadata
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_SESSION_METADATA);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_session_to_places
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_SESSION_TO_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);

    // The bookmarks roots get initialized in CheckRoots().
  }

  // Set the schema version to the current one.
  rv = mMainConn->SetSchemaVersion(DATABASE_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // ANY FAILURE IN THIS METHOD WILL CAUSE US TO MARK THE DATABASE AS CORRUPT
  // AND TRY TO REPLACE IT.
  // DO NOT PUT HERE ANYTHING THAT IS NOT RELATED TO INITIALIZATION OR MODIFYING
  // THE DISK DATABASE.

  return NS_OK;
}

nsresult Database::CheckRoots() {
  MOZ_ASSERT(NS_IsMainThread());

  // If the database has just been created, skip straight to the part where
  // we create the roots.
  if (mDatabaseStatus == nsINavHistoryService::DATABASE_STATUS_CREATE) {
    return EnsureBookmarkRoots(0, /* shouldReparentRoots */ false);
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      nsLiteralCString("SELECT guid, id, position, parent FROM moz_bookmarks "
                       "WHERE guid IN ( "
                       "'" ROOT_GUID "', '" MENU_ROOT_GUID
                       "', '" TOOLBAR_ROOT_GUID "', "
                       "'" TAGS_ROOT_GUID "', '" UNFILED_ROOT_GUID
                       "', '" MOBILE_ROOT_GUID "' )"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  nsAutoCString guid;
  int32_t maxPosition = 0;
  bool shouldReparentRoots = false;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    rv = stmt->GetUTF8String(0, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    int64_t parentId = stmt->AsInt64(3);

    if (guid.EqualsLiteral(ROOT_GUID)) {
      mRootId = stmt->AsInt64(1);
      shouldReparentRoots |= parentId != 0;
    } else {
      maxPosition = std::max(stmt->AsInt32(2), maxPosition);

      if (guid.EqualsLiteral(MENU_ROOT_GUID)) {
        mMenuRootId = stmt->AsInt64(1);
      } else if (guid.EqualsLiteral(TOOLBAR_ROOT_GUID)) {
        mToolbarRootId = stmt->AsInt64(1);
      } else if (guid.EqualsLiteral(TAGS_ROOT_GUID)) {
        mTagsRootId = stmt->AsInt64(1);
      } else if (guid.EqualsLiteral(UNFILED_ROOT_GUID)) {
        mUnfiledRootId = stmt->AsInt64(1);
      } else if (guid.EqualsLiteral(MOBILE_ROOT_GUID)) {
        mMobileRootId = stmt->AsInt64(1);
      }
      shouldReparentRoots |= parentId != mRootId;
    }
  }

  rv = EnsureBookmarkRoots(maxPosition + 1, shouldReparentRoots);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::EnsureBookmarkRoots(const int32_t startPosition,
                                       bool shouldReparentRoots) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (mRootId < 1) {
    // The first root's title is an empty string.
    rv = CreateRoot(mMainConn, "places"_ns, "root________"_ns, ""_ns, 0,
                    mRootId);

    if (NS_FAILED(rv)) return rv;
  }

  int32_t position = startPosition;

  // For the other roots, the UI doesn't rely on the value in the database, so
  // just set it to something simple to make it easier for humans to read.
  if (mMenuRootId < 1) {
    rv = CreateRoot(mMainConn, "menu"_ns, "menu________"_ns, "menu"_ns,
                    position, mMenuRootId);
    if (NS_FAILED(rv)) return rv;
    position++;
  }

  if (mToolbarRootId < 1) {
    rv = CreateRoot(mMainConn, "toolbar"_ns, "toolbar_____"_ns, "toolbar"_ns,
                    position, mToolbarRootId);
    if (NS_FAILED(rv)) return rv;
    position++;
  }

  if (mTagsRootId < 1) {
    rv = CreateRoot(mMainConn, "tags"_ns, "tags________"_ns, "tags"_ns,
                    position, mTagsRootId);
    if (NS_FAILED(rv)) return rv;
    position++;
  }

  if (mUnfiledRootId < 1) {
    rv = CreateRoot(mMainConn, "unfiled"_ns, "unfiled_____"_ns, "unfiled"_ns,
                    position, mUnfiledRootId);
    if (NS_FAILED(rv)) return rv;
    position++;
  }

  if (mMobileRootId < 1) {
    int64_t mobileRootId = CreateMobileRoot();
    if (mobileRootId <= 0) return NS_ERROR_FAILURE;
    {
      nsCOMPtr<mozIStorageStatement> mobileRootSyncStatusStmt;
      rv = mMainConn->CreateStatement(
          nsLiteralCString("UPDATE moz_bookmarks SET syncStatus = "
                           ":sync_status WHERE id = :id"),
          getter_AddRefs(mobileRootSyncStatusStmt));
      if (NS_FAILED(rv)) return rv;

      rv = mobileRootSyncStatusStmt->BindInt32ByName(
          "sync_status"_ns, nsINavBookmarksService::SYNC_STATUS_NEW);
      if (NS_FAILED(rv)) return rv;
      rv = mobileRootSyncStatusStmt->BindInt64ByName("id"_ns, mobileRootId);
      if (NS_FAILED(rv)) return rv;

      rv = mobileRootSyncStatusStmt->Execute();
      if (NS_FAILED(rv)) return rv;

      mMobileRootId = mobileRootId;
    }
  }

  if (!shouldReparentRoots) {
    return NS_OK;
  }

  // At least one root had the wrong parent, so we need to ensure that
  // all roots are parented correctly, fix their positions, and bump the
  // Sync change counter.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "CREATE TEMP TRIGGER moz_ensure_bookmark_roots_trigger "
      "AFTER UPDATE OF parent ON moz_bookmarks FOR EACH ROW "
      "WHEN OLD.parent <> NEW.parent "
      "BEGIN "
      "UPDATE moz_bookmarks SET "
      "syncChangeCounter = syncChangeCounter + 1 "
      "WHERE id IN (OLD.parent, NEW.parent, NEW.id); "

      "UPDATE moz_bookmarks SET "
      "position = position - 1 "
      "WHERE parent = OLD.parent AND position >= OLD.position; "

      // Fix the positions of the root's old siblings. Since we've already
      // moved the root, we need to exclude it from the subquery.
      "UPDATE moz_bookmarks SET "
      "position = IFNULL((SELECT MAX(position) + 1 FROM moz_bookmarks "
      "WHERE parent = NEW.parent AND "
      "id <> NEW.id), 0)"
      "WHERE id = NEW.id; "
      "END"));
  if (NS_FAILED(rv)) return rv;
  auto guard = MakeScopeExit([&]() {
    Unused << mMainConn->ExecuteSimpleSQL(
        "DROP TRIGGER moz_ensure_bookmark_roots_trigger"_ns);
  });

  nsCOMPtr<mozIStorageStatement> reparentStmt;
  rv = mMainConn->CreateStatement(
      nsLiteralCString(
          "UPDATE moz_bookmarks SET "
          "parent = CASE id WHEN :root_id THEN 0 ELSE :root_id END "
          "WHERE id IN (:root_id, :menu_root_id, :toolbar_root_id, "
          ":tags_root_id, "
          ":unfiled_root_id, :mobile_root_id)"),
      getter_AddRefs(reparentStmt));
  if (NS_FAILED(rv)) return rv;

  rv = reparentStmt->BindInt64ByName("root_id"_ns, mRootId);
  if (NS_FAILED(rv)) return rv;
  rv = reparentStmt->BindInt64ByName("menu_root_id"_ns, mMenuRootId);
  if (NS_FAILED(rv)) return rv;
  rv = reparentStmt->BindInt64ByName("toolbar_root_id"_ns, mToolbarRootId);
  if (NS_FAILED(rv)) return rv;
  rv = reparentStmt->BindInt64ByName("tags_root_id"_ns, mTagsRootId);
  if (NS_FAILED(rv)) return rv;
  rv = reparentStmt->BindInt64ByName("unfiled_root_id"_ns, mUnfiledRootId);
  if (NS_FAILED(rv)) return rv;
  rv = reparentStmt->BindInt64ByName("mobile_root_id"_ns, mMobileRootId);
  if (NS_FAILED(rv)) return rv;

  rv = reparentStmt->Execute();
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult Database::InitFunctions() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = GetUnreversedHostFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = MatchAutoCompleteFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CalculateFrecencyFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GenerateGUIDFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = IsValidGUIDFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = FixupURLFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = StoreLastInsertedIdFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = HashFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetQueryParamFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetPrefixFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GetHostAndPortFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = StripPrefixAndUserinfoFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = IsFrecencyDecayingFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NoteSyncChangeFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InvalidateDaysOfHistoryFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::InitTempEntities() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv =
      mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the triggers that update the moz_origins table as necessary.
  rv = mMainConn->ExecuteSimpleSQL(CREATE_UPDATEORIGINSINSERT_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_UPDATEORIGINSINSERT_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_UPDATEORIGINSDELETE_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_UPDATEORIGINSDELETE_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_UPDATEORIGINSUPDATE_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_UPDATEORIGINSUPDATE_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_KEYWORDS_FOREIGNCOUNT_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_KEYWORDS_FOREIGNCOUNT_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(
      CREATE_KEYWORDS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv =
      mMainConn->ExecuteSimpleSQL(CREATE_BOOKMARKS_DELETED_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv =
      mMainConn->ExecuteSimpleSQL(CREATE_BOOKMARKS_DELETED_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_METADATA_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_METADATA_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::MigrateV44Up() {
  // We need to remove any non-builtin roots and their descendants.

  // Install a temp trigger to clean up linked tables when the main
  // bookmarks are deleted.
  nsresult rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "CREATE TEMP TRIGGER moz_migrate_bookmarks_trigger "
      "AFTER DELETE ON moz_bookmarks FOR EACH ROW "
      "BEGIN "
      // Insert tombstones.
      "INSERT OR IGNORE INTO moz_bookmarks_deleted (guid, dateRemoved) "
      "VALUES (OLD.guid, strftime('%s', 'now', 'localtime', 'utc') * 1000000); "
      // Remove old annotations for the bookmarks.
      "DELETE FROM moz_items_annos "
      "WHERE item_id = OLD.id; "
      // Decrease the foreign_count in moz_places.
      "UPDATE moz_places "
      "SET foreign_count = foreign_count - 1 "
      "WHERE id = OLD.fk; "
      "END "));
  if (NS_FAILED(rv)) return rv;

  // This trigger listens for moz_places deletes, and updates moz_annos and
  // moz_keywords accordingly.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "CREATE TEMP TRIGGER moz_migrate_annos_trigger "
      "AFTER UPDATE ON moz_places FOR EACH ROW "
      // Only remove from moz_places if we don't have any remaining keywords
      // pointing to this place, and it hasn't been visited. Note: orphan
      // keywords are tidied up below.
      "WHEN NEW.visit_count = 0 AND "
      " NEW.foreign_count = (SELECT COUNT(*) FROM moz_keywords WHERE place_id "
      "= NEW.id) "
      "BEGIN "
      // No more references to the place, so we can delete the place itself.
      "DELETE FROM moz_places "
      "WHERE id = NEW.id; "
      // Delete annotations relating to the place.
      "DELETE FROM moz_annos "
      "WHERE place_id = NEW.id; "
      // Delete keywords relating to the place.
      "DELETE FROM moz_keywords "
      "WHERE place_id = NEW.id; "
      "END "));
  if (NS_FAILED(rv)) return rv;

  // Listens to moz_keyword deletions, to ensure moz_places gets the
  // foreign_count updated corrrectly.
  rv = mMainConn->ExecuteSimpleSQL(
      nsLiteralCString("CREATE TEMP TRIGGER moz_migrate_keyword_trigger "
                       "AFTER DELETE ON moz_keywords FOR EACH ROW "
                       "BEGIN "
                       // If we remove a keyword, then reduce the foreign_count.
                       "UPDATE moz_places "
                       "SET foreign_count = foreign_count - 1 "
                       "WHERE id = OLD.place_id; "
                       "END "));
  if (NS_FAILED(rv)) return rv;

  // First of all, find the non-builtin roots.
  nsCOMPtr<mozIStorageStatement> deleteStmt;
  rv = mMainConn->CreateStatement(
      nsLiteralCString("WITH RECURSIVE "
                       "itemsToRemove(id, guid) AS ( "
                       "SELECT b.id, b.guid FROM moz_bookmarks b "
                       "JOIN moz_bookmarks p ON b.parent = p.id "
                       "WHERE p.guid = 'root________' AND "
                       "b.guid NOT IN ('menu________', 'toolbar_____', "
                       "'tags________', 'unfiled_____', 'mobile______') "
                       "UNION ALL "
                       "SELECT b.id, b.guid FROM moz_bookmarks b "
                       "JOIN itemsToRemove d ON d.id = b.parent "
                       "WHERE b.guid NOT IN ('menu________', 'toolbar_____', "
                       "'tags________', 'unfiled_____', 'mobile______') "
                       ") "
                       "DELETE FROM moz_bookmarks "
                       "WHERE id IN (SELECT id FROM itemsToRemove) "),
      getter_AddRefs(deleteStmt));
  if (NS_FAILED(rv)) return rv;

  rv = deleteStmt->Execute();
  if (NS_FAILED(rv)) return rv;

  // Before we remove the triggers, check for keywords attached to places which
  // no longer have a bookmark to them. We do this before removing the triggers,
  // so that we can make use of the keyword trigger to update the counts in
  // moz_places.
  rv = mMainConn->ExecuteSimpleSQL(
      nsLiteralCString("DELETE FROM moz_keywords WHERE place_id IN ( "
                       "SELECT h.id FROM moz_keywords k "
                       "JOIN moz_places h ON h.id = k.place_id "
                       "GROUP BY place_id HAVING h.foreign_count = count(*) "
                       ")"));
  if (NS_FAILED(rv)) return rv;

  // Now remove the temp triggers.
  rv = mMainConn->ExecuteSimpleSQL(
      "DROP TRIGGER moz_migrate_bookmarks_trigger "_ns);
  if (NS_FAILED(rv)) return rv;
  rv =
      mMainConn->ExecuteSimpleSQL("DROP TRIGGER moz_migrate_annos_trigger "_ns);
  if (NS_FAILED(rv)) return rv;
  rv = mMainConn->ExecuteSimpleSQL(
      "DROP TRIGGER moz_migrate_keyword_trigger "_ns);
  if (NS_FAILED(rv)) return rv;

  // Cleanup any orphan annotation attributes.
  rv = mMainConn->ExecuteSimpleSQL(
      nsLiteralCString("DELETE FROM moz_anno_attributes WHERE id IN ( "
                       "SELECT id FROM moz_anno_attributes n "
                       "EXCEPT "
                       "SELECT DISTINCT anno_attribute_id FROM moz_annos "
                       "EXCEPT "
                       "SELECT DISTINCT anno_attribute_id FROM moz_items_annos "
                       ")"));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult Database::MigrateV45Up() {
  nsCOMPtr<mozIStorageStatement> metaTableStmt;
  nsresult rv = mMainConn->CreateStatement("SELECT 1 FROM moz_meta"_ns,
                                           getter_AddRefs(metaTableStmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_META);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult Database::MigrateV46Up() {
  // Convert the existing queries. For simplicity we assume the user didn't
  // edit these queries, and just do a 1:1 conversion.
  nsresult rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "UPDATE moz_places "
      "SET url = IFNULL('place:tag=' || ( "
      "SELECT title FROM moz_bookmarks "
      "WHERE id = CAST(get_query_param(substr(url, 7), 'folder') AS INT) "
      "), url) "
      "WHERE url_hash BETWEEN hash('place', 'prefix_lo') AND "
      "hash('place', 'prefix_hi') "
      "AND url LIKE '%type=7%' "
      "AND EXISTS(SELECT 1 FROM moz_bookmarks "
      "WHERE id = CAST(get_query_param(substr(url, 7), 'folder') AS INT)) "));

  // Recalculate hashes for all tag queries.
  rv = mMainConn->ExecuteSimpleSQL(
      nsLiteralCString("UPDATE moz_places SET url_hash = hash(url) "
                       "WHERE url_hash BETWEEN hash('place', 'prefix_lo') AND "
                       "hash('place', 'prefix_hi') "
                       "AND url LIKE '%tag=%' "));
  NS_ENSURE_SUCCESS(rv, rv);

  // Update Sync fields for all tag queries.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "UPDATE moz_bookmarks SET syncChangeCounter = syncChangeCounter + 1 "
      "WHERE fk IN ( "
      "SELECT id FROM moz_places "
      "WHERE url_hash BETWEEN hash('place', 'prefix_lo') AND "
      "hash('place', 'prefix_hi') "
      "AND url LIKE '%tag=%' "
      ") "));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult Database::MigrateV47Up() {
  // v46 may have mistakenly set some url to NULL, we must fix those.
  // Since the original url was an invalid query, we replace NULLs with an
  // empty query.
  nsresult rv = mMainConn->ExecuteSimpleSQL(
      nsLiteralCString("UPDATE moz_places "
                       "SET url = 'place:excludeItems=1', url_hash = "
                       "hash('place:excludeItems=1') "
                       "WHERE url ISNULL "));
  NS_ENSURE_SUCCESS(rv, rv);
  // Update Sync fields for these queries.
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "UPDATE moz_bookmarks SET syncChangeCounter = syncChangeCounter + 1 "
      "WHERE fk IN ( "
      "SELECT id FROM moz_places "
      "WHERE url_hash = hash('place:excludeItems=1') "
      "AND url = 'place:excludeItems=1' "
      ") "));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult Database::MigrateV48Up() {
  // Create and populate moz_origins.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement("SELECT * FROM moz_origins; "_ns,
                                           getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_ORIGINS);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "INSERT OR IGNORE INTO moz_origins (prefix, host, frecency) "
      "SELECT get_prefix(url), get_host_and_port(url), -1 "
      "FROM moz_places; "));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add and populate moz_places.origin_id.
  rv = mMainConn->CreateStatement("SELECT origin_id FROM moz_places; "_ns,
                                  getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
        "ALTER TABLE moz_places "
        "ADD COLUMN origin_id INTEGER REFERENCES moz_origins(id); "));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_ORIGIN_ID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "UPDATE moz_places "
      "SET origin_id = ( "
      "SELECT id FROM moz_origins "
      "WHERE prefix = get_prefix(url) AND host = get_host_and_port(url) "
      "); "));
  NS_ENSURE_SUCCESS(rv, rv);

  // From this point on, nobody should use moz_hosts again.  Empty it so that we
  // don't leak the user's history, but don't remove it yet so that the user can
  // downgrade.
  // This can fail, if moz_hosts doesn't exist anymore, that is what happens in
  // case of downgrade+upgrade.
  Unused << mMainConn->ExecuteSimpleSQL("DELETE FROM moz_hosts; "_ns);

  return NS_OK;
}

nsresult Database::MigrateV49Up() {
  // These hidden preferences were added along with the v48 migration as part of
  // the frecency stats implementation but are now replaced with entries in the
  // moz_meta table.
  Unused << Preferences::ClearUser("places.frecency.stats.count");
  Unused << Preferences::ClearUser("places.frecency.stats.sum");
  Unused << Preferences::ClearUser("places.frecency.stats.sumOfSquares");
  return NS_OK;
}

nsresult Database::MigrateV50Up() {
  // Convert the existing queries. We don't have REGEX available, so the
  // simplest thing to do is to pull the urls out, and process them manually.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      nsLiteralCString("SELECT id, url FROM moz_places "
                       "WHERE url_hash BETWEEN hash('place', 'prefix_lo') AND "
                       "hash('place', 'prefix_hi') "
                       "AND url LIKE '%folder=%' "),
      getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  AutoTArray<std::pair<int64_t, nsCString>, 32> placeURLs;

  bool hasMore = false;
  nsCString url;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    int64_t placeId;
    rv = stmt->GetInt64(0, &placeId);
    if (NS_FAILED(rv)) return rv;
    rv = stmt->GetUTF8String(1, url);
    if (NS_FAILED(rv)) return rv;

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    placeURLs.AppendElement(std::make_pair(placeId, url));
  }

  if (placeURLs.IsEmpty()) {
    return NS_OK;
  }

  int64_t placeId;
  for (uint32_t i = 0; i < placeURLs.Length(); ++i) {
    placeId = placeURLs[i].first;
    url = placeURLs[i].second;

    rv = ConvertOldStyleQuery(url);
    // Something bad happened, and we can't convert it, so just continue.
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsCOMPtr<mozIStorageStatement> updateStmt;
    rv = mMainConn->CreateStatement(
        nsLiteralCString("UPDATE moz_places "
                         "SET url = :url, url_hash = hash(:url) "
                         "WHERE id = :placeId "),
        getter_AddRefs(updateStmt));
    if (NS_FAILED(rv)) return rv;

    rv = URIBinder::Bind(updateStmt, "url"_ns, url);
    if (NS_FAILED(rv)) return rv;
    rv = updateStmt->BindInt64ByName("placeId"_ns, placeId);
    if (NS_FAILED(rv)) return rv;

    rv = updateStmt->Execute();
    if (NS_FAILED(rv)) return rv;

    // Update Sync fields for these queries.
    nsCOMPtr<mozIStorageStatement> syncStmt;
    rv = mMainConn->CreateStatement(
        nsLiteralCString("UPDATE moz_bookmarks SET syncChangeCounter = "
                         "syncChangeCounter + 1 "
                         "WHERE fk = :placeId "),
        getter_AddRefs(syncStmt));
    if (NS_FAILED(rv)) return rv;

    rv = syncStmt->BindInt64ByName("placeId"_ns, placeId);
    if (NS_FAILED(rv)) return rv;

    rv = syncStmt->Execute();
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

struct StringWriteFunc : public JSONWriteFunc {
  nsCString& mCString;
  explicit StringWriteFunc(nsCString& aCString) : mCString(aCString) {}
  void Write(const Span<const char>& aStr) override { mCString.Append(aStr); }
};

nsresult Database::MigrateV51Up() {
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      nsLiteralCString("SELECT b.guid FROM moz_anno_attributes n "
                       "JOIN moz_items_annos a ON n.id = a.anno_attribute_id "
                       "JOIN moz_bookmarks b ON a.item_id = b.id "
                       "WHERE n.name = :anno_name ORDER BY a.content DESC"),
      getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false,
               "Should succeed unless item annotations table has been removed");
    return NS_OK;
  };

  rv = stmt->BindUTF8StringByName("anno_name"_ns, LAST_USED_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString json;
  JSONWriter jw{MakeUnique<StringWriteFunc>(json)};
  jw.StartArrayProperty(nullptr, JSONWriter::SingleLineStyle);

  bool hasAtLeastOne = false;
  bool hasMore = false;
  uint32_t length;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    hasAtLeastOne = true;
    const char* stmtString = stmt->AsSharedUTF8String(0, &length);
    jw.StringElement(Span<const char>(stmtString, length));
  }
  jw.EndArray();

  // If we don't have any, just abort early and save the extra work.
  if (!hasAtLeastOne) {
    return NS_OK;
  }

  rv = mMainConn->CreateStatement(
      nsLiteralCString("INSERT OR REPLACE INTO moz_meta "
                       "VALUES (:key, :value) "),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindUTF8StringByName("key"_ns, LAST_USED_FOLDERS_META_KEY);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName("value"_ns, json);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clean up the now redundant annotations.
  rv = mMainConn->CreateStatement(
      nsLiteralCString(
          "DELETE FROM moz_items_annos WHERE anno_attribute_id = "
          "(SELECT id FROM moz_anno_attributes WHERE name = :anno_name) "),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName("anno_name"_ns, LAST_USED_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->CreateStatement(
      nsLiteralCString(
          "DELETE FROM moz_anno_attributes WHERE name = :anno_name "),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName("anno_name"_ns, LAST_USED_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

namespace {

class MigrateV52OriginFrecenciesRunnable final : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE
  explicit MigrateV52OriginFrecenciesRunnable(mozIStorageConnection* aDBConn);

 private:
  nsCOMPtr<mozIStorageConnection> mDBConn;
};

MigrateV52OriginFrecenciesRunnable::MigrateV52OriginFrecenciesRunnable(
    mozIStorageConnection* aDBConn)
    : Runnable("places::MigrateV52OriginFrecenciesRunnable"),
      mDBConn(aDBConn) {}

NS_IMETHODIMP
MigrateV52OriginFrecenciesRunnable::Run() {
  if (NS_IsMainThread()) {
    // Migration done.  Clear the pref.
    Unused << Preferences::ClearUser(PREF_MIGRATE_V52_ORIGIN_FRECENCIES);

    // Now that frecencies have been migrated, recalculate the origin frecency
    // stats.
    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_STATE(navHistory);
    nsresult rv = navHistory->RecalculateOriginFrecencyStats(nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // We do the work in chunks, or the wal journal may grow too much.
  nsresult rv = mDBConn->ExecuteSimpleSQL(nsLiteralCString(
      "UPDATE moz_origins "
      "SET frecency = ( "
      "SELECT CAST(TOTAL(frecency) AS INTEGER) "
      "FROM moz_places "
      "WHERE frecency > 0 AND moz_places.origin_id = moz_origins.id "
      ") "
      "WHERE id IN ( "
      "SELECT id "
      "FROM moz_origins "
      "WHERE frecency < 0 "
      "LIMIT 400 "
      ") "));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> selectStmt;
  rv = mDBConn->CreateStatement(nsLiteralCString("SELECT 1 "
                                                 "FROM moz_origins "
                                                 "WHERE frecency < 0 "
                                                 "LIMIT 1 "),
                                getter_AddRefs(selectStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  bool hasResult = false;
  rv = selectStmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasResult) {
    // There are more results to handle. Re-dispatch to the same thread for the
    // next chunk.
    return NS_DispatchToCurrentThread(this);
  }

  // Re-dispatch to the main-thread to flip the migration pref.
  return NS_DispatchToMainThread(this);
}

}  // namespace

void Database::MigrateV52OriginFrecencies() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!Preferences::GetBool(PREF_MIGRATE_V52_ORIGIN_FRECENCIES)) {
    // The migration has already been completed.
    return;
  }

  RefPtr<MigrateV52OriginFrecenciesRunnable> runnable(
      new MigrateV52OriginFrecenciesRunnable(mMainConn));
  nsCOMPtr<nsIEventTarget> target(do_GetInterface(mMainConn));
  MOZ_ASSERT(target);
  if (target) {
    Unused << target->Dispatch(runnable, NS_DISPATCH_NORMAL);
  }
}

nsresult Database::MigrateV52Up() {
  // Before this migration, moz_origin.frecency is the max frecency of all
  // places with the origin.  After this migration, it's the sum of frecencies
  // of all places with the origin.
  //
  // Setting this pref will cause InitSchema to begin async migration, via
  // MigrateV52OriginFrecencies.  When that migration is done, origin frecency
  // stats are recalculated (see MigrateV52OriginFrecenciesRunnable::Run).
  Unused << Preferences::SetBool(PREF_MIGRATE_V52_ORIGIN_FRECENCIES, true);

  // Set all origin frecencies to -1 so that MigrateV52OriginFrecenciesRunnable
  // will migrate them.
  nsresult rv =
      mMainConn->ExecuteSimpleSQL("UPDATE moz_origins SET frecency = -1 "_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  // This migration also renames these moz_meta keys that keep track of frecency
  // stats.  (That happens when stats are recalculated.)  Delete the old ones.
  rv =
      mMainConn->ExecuteSimpleSQL(nsLiteralCString("DELETE FROM moz_meta "
                                                   "WHERE key IN ( "
                                                   "'frecency_count', "
                                                   "'frecency_sum', "
                                                   "'frecency_sum_of_squares' "
                                                   ") "));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::MigrateV53Up() {
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement("SELECT 1 FROM moz_items_annos"_ns,
                                           getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    // Likely we removed the table.
    return NS_OK;
  }

  // Remove all item annotations but SYNC_PARENT_ANNO.
  rv = mMainConn->CreateStatement(
      nsLiteralCString(
          "DELETE FROM moz_items_annos "
          "WHERE anno_attribute_id NOT IN ( "
          "  SELECT id FROM moz_anno_attributes WHERE name = :anno_name "
          ") "),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName("anno_name"_ns,
                                  nsLiteralCString(SYNC_PARENT_ANNO));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(nsLiteralCString(
      "DELETE FROM moz_anno_attributes WHERE id IN ( "
      "  SELECT id FROM moz_anno_attributes "
      "  EXCEPT "
      "  SELECT DISTINCT anno_attribute_id FROM moz_annos "
      "  EXCEPT "
      "  SELECT DISTINCT anno_attribute_id FROM moz_items_annos "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::MigrateV54Up() {
  // Add an expiration column to moz_icons_to_pages.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT expire_ms FROM moz_icons_to_pages"_ns, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(
        "ALTER TABLE moz_icons_to_pages "
        "ADD COLUMN expire_ms INTEGER NOT NULL DEFAULT 0 "_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set all the zero-ed entries as expired today, they won't be removed until
  // the next related page load.
  rv = mMainConn->ExecuteSimpleSQL(
      "UPDATE moz_icons_to_pages "
      "SET expire_ms = strftime('%s','now','localtime','start "
      "of day','utc') * 1000 "
      "WHERE expire_ms = 0 "_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Database::MigrateV55Up() {
  // Add places metadata tables.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT id FROM moz_places_metadata"_ns, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    // Create the tables.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA);
    NS_ENSURE_SUCCESS(rv, rv);
    // moz_places_metadata_search_queries.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SEARCH_QUERIES);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult Database::MigrateV56Up() {
  // Add places metadata (place_id, created_at) index.
  return mMainConn->ExecuteSimpleSQL(
      CREATE_IDX_MOZ_PLACES_METADATA_PLACECREATED);
}

nsresult Database::MigrateV57Up() {
  // Add the scrolling columns to the metadata.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT scrolling_time FROM moz_places_metadata"_ns,
      getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(
        "ALTER TABLE moz_places_metadata "
        "ADD COLUMN scrolling_time INTEGER NOT NULL DEFAULT 0 "_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mMainConn->CreateStatement(
      "SELECT scrolling_distance FROM moz_places_metadata"_ns,
      getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(
        "ALTER TABLE moz_places_metadata "
        "ADD COLUMN scrolling_distance INTEGER NOT NULL DEFAULT 0 "_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult Database::MigrateV58Up() {
  // Add metadata snapshots tables if necessary.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT id FROM moz_places_metadata_snapshots"_ns, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SNAPSHOTS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv =
        mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_METADATA_SNAPSHOTS_EXTRA);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(
        CREATE_MOZ_PLACES_METADATA_SNAPSHOTS_GROUPS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(
        CREATE_MOZ_PLACES_METADATA_GROUPS_TO_SNAPSHOTS);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult Database::MigrateV59Up() {
  // Add metadata snapshots tables if necessary.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT id FROM moz_session_metadata"_ns, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_SESSION_METADATA);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_SESSION_TO_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult Database::MigrateV60Up() {
  // Add the site_name column to moz_places.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(
      "SELECT site_name FROM moz_places"_ns, getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(
        "ALTER TABLE moz_places ADD COLUMN site_name TEXT"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult Database::ConvertOldStyleQuery(nsCString& aURL) {
  AutoTArray<QueryKeyValuePair, 8> tokens;
  nsresult rv = TokenizeQueryString(aURL, &tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoTArray<QueryKeyValuePair, 8> newTokens;
  bool invalid = false;
  nsAutoCString guid;

  for (uint32_t j = 0; j < tokens.Length(); ++j) {
    const QueryKeyValuePair& kvp = tokens[j];

    if (!kvp.key.EqualsLiteral("folder")) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      newTokens.AppendElement(kvp);
      continue;
    }

    int64_t itemId = kvp.value.ToInteger(&rv);
    if (NS_SUCCEEDED(rv)) {
      // We have the folder's ID, now to find its GUID.
      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = mMainConn->CreateStatement(
          nsLiteralCString("SELECT guid FROM moz_bookmarks "
                           "WHERE id = :itemId "),
          getter_AddRefs(stmt));
      if (NS_FAILED(rv)) return rv;

      rv = stmt->BindInt64ByName("itemId"_ns, itemId);
      if (NS_FAILED(rv)) return rv;

      bool hasMore = false;
      if (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
        rv = stmt->GetUTF8String(0, guid);
        if (NS_FAILED(rv)) return rv;
      }
    } else if (kvp.value.EqualsLiteral("PLACES_ROOT")) {
      guid = nsLiteralCString(ROOT_GUID);
    } else if (kvp.value.EqualsLiteral("BOOKMARKS_MENU")) {
      guid = nsLiteralCString(MENU_ROOT_GUID);
    } else if (kvp.value.EqualsLiteral("TAGS")) {
      guid = nsLiteralCString(TAGS_ROOT_GUID);
    } else if (kvp.value.EqualsLiteral("UNFILED_BOOKMARKS")) {
      guid = nsLiteralCString(UNFILED_ROOT_GUID);
    } else if (kvp.value.EqualsLiteral("TOOLBAR")) {
      guid = nsLiteralCString(TOOLBAR_ROOT_GUID);
    } else if (kvp.value.EqualsLiteral("MOBILE_BOOKMARKS")) {
      guid = nsLiteralCString(MOBILE_ROOT_GUID);
    }

    QueryKeyValuePair* newPair;
    if (guid.IsEmpty()) {
      // This is invalid, so we'll change this key/value pair to something else
      // so that the query remains a valid url.
      newPair = new QueryKeyValuePair("invalidOldParentId"_ns, kvp.value);
      invalid = true;
    } else {
      newPair = new QueryKeyValuePair("parent"_ns, guid);
    }
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    newTokens.AppendElement(*newPair);
    delete newPair;
  }

  if (invalid) {
    // One or more of the folders don't exist, replace with an empty query.
    newTokens.AppendElement(QueryKeyValuePair("excludeItems"_ns, "1"_ns));
  }

  TokensToQueryString(newTokens, aURL);
  return NS_OK;
}

int64_t Database::CreateMobileRoot() {
  MOZ_ASSERT(NS_IsMainThread());

  // Create the mobile root, ignoring conflicts if one already exists (for
  // example, if the user downgraded to an earlier release channel).
  nsCOMPtr<mozIStorageStatement> createStmt;
  nsresult rv = mMainConn->CreateStatement(
      nsLiteralCString(
          "INSERT OR IGNORE INTO moz_bookmarks "
          "(type, title, dateAdded, lastModified, guid, position, parent) "
          "SELECT :item_type, :item_title, :timestamp, :timestamp, :guid, "
          "IFNULL((SELECT MAX(position) + 1 FROM moz_bookmarks p WHERE "
          "p.parent = b.id), 0), b.id "
          "FROM moz_bookmarks b WHERE b.parent = 0"),
      getter_AddRefs(createStmt));
  if (NS_FAILED(rv)) return -1;

  rv = createStmt->BindInt32ByName("item_type"_ns,
                                   nsINavBookmarksService::TYPE_FOLDER);
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindUTF8StringByName("item_title"_ns,
                                        nsLiteralCString(MOBILE_ROOT_TITLE));
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindInt64ByName("timestamp"_ns, RoundedPRNow());
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindUTF8StringByName("guid"_ns,
                                        nsLiteralCString(MOBILE_ROOT_GUID));
  if (NS_FAILED(rv)) return -1;

  rv = createStmt->Execute();
  if (NS_FAILED(rv)) return -1;

  // Find the mobile root ID. We can't use the last inserted ID because the
  // root might already exist, and we ignore on conflict.
  nsCOMPtr<mozIStorageStatement> findIdStmt;
  rv = mMainConn->CreateStatement(
      "SELECT id FROM moz_bookmarks WHERE guid = :guid"_ns,
      getter_AddRefs(findIdStmt));
  if (NS_FAILED(rv)) return -1;

  rv = findIdStmt->BindUTF8StringByName("guid"_ns,
                                        nsLiteralCString(MOBILE_ROOT_GUID));
  if (NS_FAILED(rv)) return -1;

  bool hasResult = false;
  rv = findIdStmt->ExecuteStep(&hasResult);
  if (NS_FAILED(rv) || !hasResult) return -1;

  int64_t rootId;
  rv = findIdStmt->GetInt64(0, &rootId);
  if (NS_FAILED(rv)) return -1;

  return rootId;
}

void Database::Shutdown() {
  // As the last step in the shutdown path, finalize the database handle.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mClosed);

  // Break cycles with the shutdown blockers.
  mClientsShutdown = nullptr;
  nsCOMPtr<mozIStorageCompletionCallback> connectionShutdown =
      std::move(mConnectionShutdown);

  if (!mMainConn) {
    // The connection has never been initialized. Just mark it as closed.
    mClosed = true;
    (void)connectionShutdown->Complete(NS_OK, nullptr);
    return;
  }

#ifdef DEBUG
  {
    bool hasResult;
    nsCOMPtr<mozIStorageStatement> stmt;

    // Sanity check for missing guids.
    nsresult rv =
        mMainConn->CreateStatement(nsLiteralCString("SELECT 1 "
                                                    "FROM moz_places "
                                                    "WHERE guid IS NULL "),
                                   getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a page without a GUID!");
    rv = mMainConn->CreateStatement(nsLiteralCString("SELECT 1 "
                                                     "FROM moz_bookmarks "
                                                     "WHERE guid IS NULL "),
                                    getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a bookmark without a GUID!");

    // Sanity check for unrounded dateAdded and lastModified values (bug
    // 1107308).
    rv = mMainConn->CreateStatement(
        nsLiteralCString(
            "SELECT 1 "
            "FROM moz_bookmarks "
            "WHERE dateAdded % 1000 > 0 OR lastModified % 1000 > 0 LIMIT 1"),
        getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found unrounded dates!");

    // Sanity check url_hash
    rv = mMainConn->CreateStatement(
        "SELECT 1 FROM moz_places WHERE url_hash = 0"_ns, getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a place without a hash!");

    // Sanity check unique urls
    rv = mMainConn->CreateStatement(
        nsLiteralCString(
            "SELECT 1 FROM moz_places GROUP BY url HAVING count(*) > 1 "),
        getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a duplicate url!");

    // Sanity check NULL urls
    rv = mMainConn->CreateStatement(
        "SELECT 1 FROM moz_places WHERE url ISNULL "_ns, getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a NULL url!");
  }
#endif

  mMainThreadStatements.FinalizeStatements();
  mMainThreadAsyncStatements.FinalizeStatements();

  RefPtr<FinalizeStatementCacheProxy<mozIStorageStatement>> event =
      new FinalizeStatementCacheProxy<mozIStorageStatement>(
          mAsyncThreadStatements, NS_ISUPPORTS_CAST(nsIObserver*, this));
  DispatchToAsyncThread(event);

  mClosed = true;

  // Execute PRAGMA optimized as last step, this will ensure proper database
  // performance across restarts.
  nsCOMPtr<mozIStoragePendingStatement> ps;
  MOZ_ALWAYS_SUCCEEDS(mMainConn->ExecuteSimpleSQLAsync(
      "PRAGMA optimize(0x02)"_ns, nullptr, getter_AddRefs(ps)));

  (void)mMainConn->AsyncClose(connectionShutdown);
  mMainConn = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
Database::Observe(nsISupports* aSubject, const char* aTopic,
                  const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  if (strcmp(aTopic, TOPIC_PROFILE_CHANGE_TEARDOWN) == 0) {
    // Tests simulating shutdown may cause multiple notifications.
    if (PlacesShutdownBlocker::sIsStarted) {
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    NS_ENSURE_STATE(os);

    // If shutdown happens in the same mainthread loop as init, observers could
    // handle the places-init-complete notification after xpcom-shutdown, when
    // the connection does not exist anymore.  Removing those observers would
    // be less expensive but may cause their RemoveObserver calls to throw.
    // Thus notify the topic now, so they stop listening for it.
    nsCOMPtr<nsISimpleEnumerator> e;
    if (NS_SUCCEEDED(os->EnumerateObservers(TOPIC_PLACES_INIT_COMPLETE,
                                            getter_AddRefs(e))) &&
        e) {
      bool hasMore = false;
      while (NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        if (NS_SUCCEEDED(e->GetNext(getter_AddRefs(supports)))) {
          nsCOMPtr<nsIObserver> observer = do_QueryInterface(supports);
          (void)observer->Observe(observer, TOPIC_PLACES_INIT_COMPLETE,
                                  nullptr);
        }
      }
    }

    // Notify all Places users that we are about to shutdown.
    (void)os->NotifyObservers(nullptr, TOPIC_PLACES_SHUTDOWN, nullptr);
  } else if (strcmp(aTopic, TOPIC_SIMULATE_PLACES_SHUTDOWN) == 0) {
    // This notification is (and must be) only used by tests that are trying
    // to simulate Places shutdown out of the normal shutdown path.

    // Tests simulating shutdown may cause re-entrance.
    if (PlacesShutdownBlocker::sIsStarted) {
      return NS_OK;
    }

    // We are simulating a shutdown, so invoke the shutdown blockers,
    // wait for them, then proceed with connection shutdown.
    // Since we are already going through shutdown, but it's not the real one,
    // we won't need to block the real one anymore, so we can unblock it.
    {
      nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
          GetProfileChangeTeardownPhase();
      if (shutdownPhase) {
        shutdownPhase->RemoveBlocker(mClientsShutdown.get());
      }
      (void)mClientsShutdown->BlockShutdown(nullptr);
    }

    // Spin the events loop until the clients are done.
    // Note, this is just for tests, specifically test_clearHistory_shutdown.js
    SpinEventLoopUntil("places:Database::Observe(SIMULATE_PLACES_SHUTDOWN)"_ns,
                       [&]() {
                         return mClientsShutdown->State() ==
                                PlacesShutdownBlocker::States::RECEIVED_DONE;
                       });

    {
      nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase =
          GetProfileBeforeChangePhase();
      if (shutdownPhase) {
        shutdownPhase->RemoveBlocker(mConnectionShutdown.get());
      }
      (void)mConnectionShutdown->BlockShutdown(nullptr);
    }
  }
  return NS_OK;
}

}  // namespace places
}  // namespace mozilla
