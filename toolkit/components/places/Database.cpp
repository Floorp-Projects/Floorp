/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"

#include "Database.h"

#include "nsIAnnotationService.h"
#include "nsINavBookmarksService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFile.h"
#include "nsIWritablePropertyBag2.h"

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
#include "prtime.h"

#include "nsXULAppAPI.h"

// Time between corrupt database backups.
#define RECENT_BACKUP_TIME_MICROSEC (int64_t)86400 * PR_USEC_PER_SEC // 24H

// Filename of the database.
#define DATABASE_FILENAME NS_LITERAL_STRING("places.sqlite")
// Filename used to backup corrupt databases.
#define DATABASE_CORRUPT_FILENAME NS_LITERAL_STRING("places.sqlite.corrupt")
// Filename of the icons database.
#define DATABASE_FAVICONS_FILENAME NS_LITERAL_STRING("favicons.sqlite")

// Set when the database file was found corrupt by a previous maintenance.
#define PREF_FORCE_DATABASE_REPLACEMENT "places.database.replaceOnStartup"

// Set to specify the size of the places database growth increments in kibibytes
#define PREF_GROWTH_INCREMENT_KIB "places.database.growthIncrementKiB"

// Set to disable the default robust storage and use volatile, in-memory
// storage without robust transaction flushing guarantees. This makes
// SQLite use much less I/O at the cost of losing data when things crash.
// The pref is only honored if an environment variable is set. The env
// variable is intentionally named something scary to help prevent someone
// from thinking it is a useful performance optimization they should enable.
#define PREF_DISABLE_DURABILITY "places.database.disableDurability"
#define ENV_ALLOW_CORRUPTION "ALLOW_PLACES_DATABASE_TO_LOSE_DATA_AND_BECOME_CORRUPT"

// The maximum url length we can store in history.
// We do not add to history URLs longer than this value.
#define PREF_HISTORY_MAXURLLEN "places.history.maxUrlLength"
// This number is mostly a guess based on various facts:
// * IE didn't support urls longer than 2083 chars
// * Sitemaps protocol used to support a maximum of 2048 chars
// * Various SEO guides suggest to not go over 2000 chars
// * Various apps/services are known to have issues over 2000 chars
// * RFC 2616 - HTTP/1.1 suggests being cautious about depending
//   on URI lengths above 255 bytes
#define PREF_HISTORY_MAXURLLEN_DEFAULT 2000

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

// Old Sync GUID annotation.
#define SYNCGUID_ANNO NS_LITERAL_CSTRING("sync/guid")

// Places string bundle, contains internationalized bookmark root names.
#define PLACES_BUNDLE "chrome://places/locale/places.properties"

// Livemarks annotations.
#define LMANNO_FEEDURI "livemark/feedURI"
#define LMANNO_SITEURI "livemark/siteURI"

#define MOBILE_ROOT_GUID "mobile______"
#define MOBILE_ROOT_ANNO "mobile/bookmarksRoot"

// We use a fixed title for the mobile root to avoid marking the database as
// corrupt if we can't look up the localized title in the string bundle. Sync
// sets the title to the localized version when it creates the left pane query.
#define MOBILE_ROOT_TITLE "mobile"

using namespace mozilla;

namespace mozilla {
namespace places {

namespace {

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Checks whether exists a database backup created not longer than
 * RECENT_BACKUP_TIME_MICROSEC ago.
 */
bool
hasRecentCorruptDB()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> profDir;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profDir));
  NS_ENSURE_TRUE(profDir, false);
  nsCOMPtr<nsISimpleEnumerator> entries;
  profDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_TRUE(entries, false);
  bool hasMore;
  while (NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> next;
    entries->GetNext(getter_AddRefs(next));
    NS_ENSURE_TRUE(next, false);
    nsCOMPtr<nsIFile> currFile = do_QueryInterface(next);
    NS_ENSURE_TRUE(currFile, false);

    nsAutoString leafName;
    if (NS_SUCCEEDED(currFile->GetLeafName(leafName)) &&
        leafName.Length() >= DATABASE_CORRUPT_FILENAME.Length() &&
        leafName.Find(".corrupt", DATABASE_FILENAME.Length()) != -1) {
      PRTime lastMod = 0;
      currFile->GetLastModifiedTime(&lastMod);
      NS_ENSURE_TRUE(lastMod > 0, false);
      return (PR_Now() - lastMod) > RECENT_BACKUP_TIME_MICROSEC;
    }
  }
  return false;
}

/**
 * Updates sqlite_stat1 table through ANALYZE.
 * Since also nsPlacesExpiration.js executes ANALYZE, the analyzed tables
 * must be the same in both components.  So ensure they are in sync.
 *
 * @param aDBConn
 *        The database connection.
 */
nsresult
updateSQLiteStatistics(mozIStorageConnection* aDBConn)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<mozIStorageAsyncStatement> analyzePlacesStmt;
  aDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "ANALYZE moz_places"
  ), getter_AddRefs(analyzePlacesStmt));
  NS_ENSURE_STATE(analyzePlacesStmt);
  nsCOMPtr<mozIStorageAsyncStatement> analyzeBookmarksStmt;
  aDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "ANALYZE moz_bookmarks"
  ), getter_AddRefs(analyzeBookmarksStmt));
  NS_ENSURE_STATE(analyzeBookmarksStmt);
  nsCOMPtr<mozIStorageAsyncStatement> analyzeVisitsStmt;
  aDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "ANALYZE moz_historyvisits"
  ), getter_AddRefs(analyzeVisitsStmt));
  NS_ENSURE_STATE(analyzeVisitsStmt);
  nsCOMPtr<mozIStorageAsyncStatement> analyzeInputStmt;
  aDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "ANALYZE moz_inputhistory"
  ), getter_AddRefs(analyzeInputStmt));
  NS_ENSURE_STATE(analyzeInputStmt);

  mozIStorageBaseStatement *stmts[] = {
    analyzePlacesStmt,
    analyzeBookmarksStmt,
    analyzeVisitsStmt,
    analyzeInputStmt
  };

  nsCOMPtr<mozIStoragePendingStatement> ps;
  (void)aDBConn->ExecuteAsync(stmts, ArrayLength(stmts), nullptr,
                              getter_AddRefs(ps));
  return NS_OK;
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
enum JournalMode
SetJournalMode(nsCOMPtr<mozIStorageConnection>& aDBConn,
               enum JournalMode aJournalMode)
{
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

nsresult
CreateRoot(nsCOMPtr<mozIStorageConnection>& aDBConn,
           const nsCString& aRootName, const nsCString& aGuid,
           const nsXPIDLString& titleString)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The position of the new item in its folder.
  static int32_t itemPosition = 0;

  // A single creation timestamp for all roots so that the root folder's
  // last modification time isn't earlier than its childrens' creation time.
  static PRTime timestamp = 0;
  if (!timestamp)
    timestamp = RoundedPRNow();

  // Create a new bookmark folder for the root.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks "
      "(type, position, title, dateAdded, lastModified, guid, parent, "
       "syncChangeCounter, syncStatus) "
    "VALUES (:item_type, :item_position, :item_title,"
            ":date_added, :last_modified, :guid, "
            "IFNULL((SELECT id FROM moz_bookmarks WHERE parent = 0), 0), "
            "1, :sync_status)"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"),
                             nsINavBookmarksService::TYPE_FOLDER);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_position"), itemPosition);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"),
                                  NS_ConvertUTF16toUTF8(titleString));
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("date_added"), timestamp);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("last_modified"), timestamp);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), aGuid);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("sync_status"),
                             nsINavBookmarksService::SYNC_STATUS_NEW);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->Execute();
  if (NS_FAILED(rv)) return rv;

  // The 'places' root is a folder containing the other roots.
  // The first bookmark in a folder has position 0.
  if (!aRootName.EqualsLiteral("places"))
    ++itemPosition;

  return NS_OK;
}

nsresult
SetupDurability(nsCOMPtr<mozIStorageConnection>& aDBConn, int32_t aDBPageSize) {
  nsresult rv;
  if (PR_GetEnv(ENV_ALLOW_CORRUPTION) &&
      Preferences::GetBool(PREF_DISABLE_DURABILITY, false)) {
    // Volatile storage was requested. Use the in-memory journal (no
    // filesystem I/O) and don't sync the filesystem after writing.
    SetJournalMode(aDBConn, JOURNAL_MEMORY);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA synchronous = OFF"));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Be sure to set journal mode after page_size.  WAL would prevent the change
    // otherwise.
    if (JOURNAL_WAL == SetJournalMode(aDBConn, JOURNAL_WAL)) {
      // Set the WAL journal size limit.
      int32_t checkpointPages =
        static_cast<int32_t>(DATABASE_MAX_WAL_BYTES / aDBPageSize);
      nsAutoCString checkpointPragma("PRAGMA wal_autocheckpoint = ");
      checkpointPragma.AppendInt(checkpointPages);
      rv = aDBConn->ExecuteSimpleSQL(checkpointPragma);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Ignore errors, if we fail here the database could be considered corrupt
      // and we won't be able to go on, even if it's just matter of a bogus file
      // system.  The default mode (DELETE) will be fine in such a case.
      (void)SetJournalMode(aDBConn, JOURNAL_TRUNCATE);

      // Set synchronous to FULL to ensure maximum data integrity, even in
      // case of crashes or unclean shutdowns.
      rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "PRAGMA synchronous = FULL"));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // The journal is usually free to grow for performance reasons, but it never
  // shrinks back.  Since the space taken may be problematic, limit its size.
  nsAutoCString journalSizePragma("PRAGMA journal_size_limit = ");
  journalSizePragma.AppendInt(DATABASE_MAX_WAL_BYTES + DATABASE_JOURNAL_OVERHEAD_BYTES);
  (void)aDBConn->ExecuteSimpleSQL(journalSizePragma);

  // Grow places in |growthIncrementKiB| increments to limit fragmentation on disk.
  // By default, it's 5 MB.
  int32_t growthIncrementKiB =
    Preferences::GetInt(PREF_GROWTH_INCREMENT_KIB, 5 * BYTES_PER_KIBIBYTE);
  if (growthIncrementKiB > 0) {
    (void)aDBConn->SetGrowthIncrement(growthIncrementKiB * BYTES_PER_KIBIBYTE, EmptyCString());
  }
  return NS_OK;
}

nsresult
AttachDatabase(nsCOMPtr<mozIStorageConnection>& aDBConn,
               const nsACString& aPath,
               const nsACString& aName) {
  nsresult rv = aDBConn->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("ATTACH DATABASE '") + aPath + NS_LITERAL_CSTRING("' AS ") + aName);
  NS_ENSURE_SUCCESS(rv, rv);

  // The journal limit must be set apart for each database.
  nsAutoCString journalSizePragma("PRAGMA favicons.journal_size_limit = ");
  journalSizePragma.AppendInt(DATABASE_MAX_WAL_BYTES + DATABASE_JOURNAL_OVERHEAD_BYTES);
  Unused << aDBConn->ExecuteSimpleSQL(journalSizePragma);

  return NS_OK;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
//// Database

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(Database, gDatabase)

NS_IMPL_ISUPPORTS(Database
, nsIObserver
, nsISupportsWeakReference
)

Database::Database()
  : mMainThreadStatements(mMainConn)
  , mMainThreadAsyncStatements(mMainConn)
  , mAsyncThreadStatements(mMainConn)
  , mDBPageSize(0)
  , mDatabaseStatus(nsINavHistoryService::DATABASE_STATUS_OK)
  , mClosed(false)
  , mClientsShutdown(new ClientsShutdownBlocker())
  , mConnectionShutdown(new ConnectionShutdownBlocker(this))
  , mMaxUrlLength(0)
{
  MOZ_ASSERT(!XRE_IsContentProcess(),
             "Cannot instantiate Places in the content process");
  // Attempting to create two instances of the service?
  MOZ_ASSERT(!gDatabase);
  gDatabase = this;
}

already_AddRefed<nsIAsyncShutdownClient>
Database::GetProfileChangeTeardownPhase()
{
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc = services::GetAsyncShutdown();
  MOZ_ASSERT(asyncShutdownSvc);
  if (NS_WARN_IF(!asyncShutdownSvc)) {
    return nullptr;
  }

  // Consumers of Places should shutdown before us, at profile-change-teardown.
  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  DebugOnly<nsresult> rv = asyncShutdownSvc->
    GetProfileChangeTeardown(getter_AddRefs(shutdownPhase));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}

already_AddRefed<nsIAsyncShutdownClient>
Database::GetProfileBeforeChangePhase()
{
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdownSvc = services::GetAsyncShutdown();
  MOZ_ASSERT(asyncShutdownSvc);
  if (NS_WARN_IF(!asyncShutdownSvc)) {
    return nullptr;
  }

  // Consumers of Places should shutdown before us, at profile-change-teardown.
  nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase;
  DebugOnly<nsresult> rv = asyncShutdownSvc->
    GetProfileBeforeChange(getter_AddRefs(shutdownPhase));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return shutdownPhase.forget();
}

Database::~Database()
{
}

bool
Database::IsShutdownStarted() const
{
  if (!mConnectionShutdown) {
    // We have already broken the cycle between `this` and `mConnectionShutdown`.
    return true;
  }
  return mConnectionShutdown->IsStarted();
}

already_AddRefed<mozIStorageAsyncStatement>
Database::GetAsyncStatement(const nsACString& aQuery) const
{
  if (IsShutdownStarted()) {
    return nullptr;
  }
  MOZ_ASSERT(NS_IsMainThread());
  return mMainThreadAsyncStatements.GetCachedStatement(aQuery);
}

already_AddRefed<mozIStorageStatement>
Database::GetStatement(const nsACString& aQuery) const
{
  if (IsShutdownStarted()) {
    return nullptr;
  }
  if (NS_IsMainThread()) {
    return mMainThreadStatements.GetCachedStatement(aQuery);
  }
  return mAsyncThreadStatements.GetCachedStatement(aQuery);
}

already_AddRefed<nsIAsyncShutdownClient>
Database::GetClientsShutdown()
{
  MOZ_ASSERT(mClientsShutdown);
  return mClientsShutdown->GetClient();
}

// static
already_AddRefed<Database>
Database::GetDatabase()
{
  if (PlacesShutdownBlocker::IsStarted()) {
    return nullptr;
  }
  return GetSingleton();
}

nsresult
Database::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool initSucceeded = false;
  auto guard = MakeScopeExit([&]() {
    if (!initSucceeded) {
      // If we bail out early here without doing anything else, the Database object
      // will leak due to the cycle between the shutdown blockers and itself, so
      // let's break the cycle first.
      Shutdown(false);
    }
  });

  nsCOMPtr<mozIStorageService> storage =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_STATE(storage);

  // Init the database file and connect to it.
  bool databaseCreated = false;
  nsresult rv = InitDatabaseFile(storage, &databaseCreated);
  if (NS_SUCCEEDED(rv) && databaseCreated) {
    mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CREATE;
  }
  else if (rv == NS_ERROR_FILE_CORRUPTED) {
    // The database is corrupt, backup and replace it with a new one.
    mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CORRUPT;
    rv = BackupAndReplaceDatabaseFile(storage);
    // Fallback to catch-all handler, that notifies a database locked failure.
  }

  // If the database connection still cannot be opened, it may just be locked
  // by third parties.  Send out a notification and interrupt initialization.
  if (NS_FAILED(rv)) {
    RefPtr<PlacesEvent> lockedEvent = new PlacesEvent(TOPIC_DATABASE_LOCKED);
    (void)NS_DispatchToMainThread(lockedEvent);
    return rv;
  }

  // Ensure the icons database exists.
  rv = EnsureFaviconsDatabaseFile(storage);
  if (NS_FAILED(rv)) {
    RefPtr<PlacesEvent> lockedEvent = new PlacesEvent(TOPIC_DATABASE_LOCKED);
    (void)NS_DispatchToMainThread(lockedEvent);
    return rv;
  }

  // Initialize the database schema.  In case of failure the existing schema is
  // is corrupt or incoherent, thus the database should be replaced.
  bool databaseMigrated = false;
  rv = SetupDatabaseConnection(storage);
  if (NS_SUCCEEDED(rv)) {
    // Failing to initialize the schema always indicates a corruption.
    if (NS_FAILED(InitSchema(&databaseMigrated))) {
      rv = NS_ERROR_FILE_CORRUPTED;
    }
  }
  if (NS_FAILED(rv)) {
    mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CORRUPT;
    // Some errors may not indicate a database corruption, for those cases we
    // just bail out without throwing away a possibly valid places.sqlite.
    if (rv == NS_ERROR_FILE_CORRUPTED) {
      rv = BackupAndReplaceDatabaseFile(storage);
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

  if (mDatabaseStatus != nsINavHistoryService::DATABASE_STATUS_OK) {
    rv = updateSQLiteStatistics(MainConn());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Initialize here all the items that are not part of the on-disk database,
  // like views, temp triggers or temp tables.  The database should not be
  // considered corrupt if any of the following fails.

  rv = InitTempEntities();
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify we have finished database initialization.
  // Enqueue the notification, so if we init another service that requires
  // nsNavHistoryService we don't recursive try to get it.
  RefPtr<PlacesEvent> completeEvent =
    new PlacesEvent(TOPIC_PLACES_INIT_COMPLETE);
  rv = NS_DispatchToMainThread(completeEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // At this point we know the Database object points to a valid connection
  // and we need to setup async shutdown.
  initSucceeded = true;
  {
    // First of all Places clients should block profile-change-teardown.
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetProfileChangeTeardownPhase();
    MOZ_ASSERT(shutdownPhase);
    if (shutdownPhase) {
      DebugOnly<nsresult> rv = shutdownPhase->AddBlocker(
        static_cast<nsIAsyncShutdownBlocker*>(mClientsShutdown.get()),
        NS_LITERAL_STRING(__FILE__),
        __LINE__,
        NS_LITERAL_STRING(""));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  {
    // Then connection closing should block profile-before-change.
    nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetProfileBeforeChangePhase();
    MOZ_ASSERT(shutdownPhase);
    if (shutdownPhase) {
      DebugOnly<nsresult> rv = shutdownPhase->AddBlocker(
        static_cast<nsIAsyncShutdownBlocker*>(mConnectionShutdown.get()),
        NS_LITERAL_STRING(__FILE__),
        __LINE__,
        NS_LITERAL_STRING(""));
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

nsresult
Database::EnsureFaviconsDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> databaseFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(databaseFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = databaseFile->Append(DATABASE_FAVICONS_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  bool databaseFileExists = false;
  rv = databaseFile->Exists(&databaseFileExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (databaseFileExists) {
    return NS_OK;
  }

  // Open the database file, this will also create it.
  nsCOMPtr<mozIStorageConnection> conn;
  rv = aStorage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(conn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure we'll close the connection when done.
  auto cleanup = MakeScopeExit([&] () {
    // We cannot use AsyncClose() here, because by the time we try to ATTACH
    // this database, its transaction could be still be running and that would
    // cause the ATTACH query to fail.
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(conn->Close()));
  });

  int32_t defaultPageSize;
  rv = conn->GetDefaultPageSize(&defaultPageSize);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetupDurability(conn, defaultPageSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Enable incremental vacuum for this database. Since it will contain even
  // large blobs and can be cleared with history, it's worth to have it.
  // Note that it will be necessary to manually use PRAGMA incremental_vacuum.
  rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA auto_vacuum = INCREMENTAL"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // We are going to update the database, so everything from now on should be
  // in a transaction for performances.
  mozStorageTransaction transaction(conn, false);
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

  return NS_OK;
}

nsresult
Database::InitDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage,
                           bool* aNewDatabaseCreated)
{
  MOZ_ASSERT(NS_IsMainThread());
  *aNewDatabaseCreated = false;

  nsCOMPtr<nsIFile> databaseFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(databaseFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = databaseFile->Append(DATABASE_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  bool databaseFileExists = false;
  rv = databaseFile->Exists(&databaseFileExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (databaseFileExists &&
      Preferences::GetBool(PREF_FORCE_DATABASE_REPLACEMENT, false)) {
    // If this pref is set, Maintenance required a database replacement, due to
    // integrity corruption.
    // Be sure to clear the pref to avoid handling it more than once.
    (void)Preferences::ClearUser(PREF_FORCE_DATABASE_REPLACEMENT);

    return NS_ERROR_FILE_CORRUPTED;
  }

  // Open the database file.  If it does not exist a new one will be created.
  // Use an unshared connection, it will consume more memory but avoid shared
  // cache contentions across threads.
  rv = aStorage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(mMainConn));
  NS_ENSURE_SUCCESS(rv, rv);

  *aNewDatabaseCreated = !databaseFileExists;
  return NS_OK;
}

nsresult
Database::BackupAndReplaceDatabaseFile(nsCOMPtr<mozIStorageService>& aStorage)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIFile> databaseFile;
  rv = profDir->Clone(getter_AddRefs(databaseFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = databaseFile->Append(DATABASE_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we have
  // already failed in the last 24 hours avoid to create another corrupt file,
  // since doing so, in some situation, could cause us to create a new corrupt
  // file at every try to access any Places service.  That is bad because it
  // would quickly fill the user's disk space without any notice.
  if (!hasRecentCorruptDB()) {
    nsCOMPtr<nsIFile> backup;
    (void)aStorage->BackupDatabaseFile(databaseFile, DATABASE_CORRUPT_FILENAME,
                                       profDir, getter_AddRefs(backup));
  }

  // If anything fails from this point on, we have a stale connection or
  // database file, and there's not much more we can do.
  // The only thing we can try to do is to replace the database on the next
  // start, and enforce a crash, so it gets reported to us.

  // Close database connection if open.
  if (mMainConn) {
    rv = mMainConn->SpinningSynchronousClose();
    NS_ENSURE_SUCCESS(rv, ForceCrashAndReplaceDatabase(
      NS_LITERAL_CSTRING("Unable to close the corrupt database.")));
  }

  // Remove the broken database.
  rv = databaseFile->Remove(false);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    return ForceCrashAndReplaceDatabase(
      NS_LITERAL_CSTRING("Unable to remove the corrupt database file."));
  }

  // Create a new database file.
  // Use an unshared connection, it will consume more memory but avoid shared
  // cache contentions across threads.
  rv = aStorage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(mMainConn));
  NS_ENSURE_SUCCESS(rv, ForceCrashAndReplaceDatabase(
    NS_LITERAL_CSTRING("Unable to open a new database connection.")));

  return NS_OK;
}

nsresult
Database::ForceCrashAndReplaceDatabase(const nsCString& aReason)
{
  Preferences::SetBool(PREF_FORCE_DATABASE_REPLACEMENT, true);
  // Ensure that prefs get saved, or we could crash before storing them.
  nsIPrefService* prefService = Preferences::GetService();
  if (prefService && NS_SUCCEEDED(static_cast<Preferences *>(prefService)->SavePrefFileBlocking())) {
    // We could force an application restart here, but we'd like to get these
    // cases reported to us, so let's force a crash instead.
    MOZ_CRASH_UNSAFE_OOL(aReason.get());
  }
  return NS_ERROR_FAILURE;
}

nsresult
Database::SetupDatabaseConnection(nsCOMPtr<mozIStorageService>& aStorage)
{
  MOZ_ASSERT(NS_IsMainThread());

  // WARNING: any statement executed before setting the journal mode must be
  // finalized, since SQLite doesn't allow changing the journal mode if there
  // is any outstanding statement.

  {
    // Get the page size.  This may be different than the default if the
    // database file already existed with a different page size.
    nsCOMPtr<mozIStorageStatement> statement;
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA page_size"
    ), getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    bool hasResult = false;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FILE_CORRUPTED);
    rv = statement->GetInt32(0, &mDBPageSize);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && mDBPageSize > 0, NS_ERROR_FILE_CORRUPTED);
  }

  // Ensure that temp tables are held in memory, not on disk.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA temp_store = MEMORY")
  );
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetupDurability(mMainConn, mDBPageSize);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString busyTimeoutPragma("PRAGMA busy_timeout = ");
  busyTimeoutPragma.AppendInt(DATABASE_BUSY_TIMEOUT_MS);
  (void)mMainConn->ExecuteSimpleSQL(busyTimeoutPragma);

  // Enable FOREIGN KEY support. This is a strict requirement.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA foreign_keys = ON")
  );
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FILE_CORRUPTED);
#ifdef DEBUG
  {
    // There are a few cases where setting foreign_keys doesn't work:
    //  * in the middle of a multi-statement transaction
    //  * if the SQLite library in use doesn't support them
    // Since we need foreign_keys, let's at least assert in debug mode.
    nsCOMPtr<mozIStorageStatement> stmt;
    mMainConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA foreign_keys"),
                            getter_AddRefs(stmt));
    bool hasResult = false;
    if (stmt && NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
      int32_t fkState = stmt->AsInt32(0);
      MOZ_ASSERT(fkState, "Foreign keys should be enabled");
    }
  }
#endif

  // Attach the favicons database to the main connection.
  nsCOMPtr<nsIFile> iconsFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(iconsFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = iconsFile->Append(DATABASE_FAVICONS_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);
  nsString iconsPath;
  rv = iconsFile->GetPath(iconsPath);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = AttachDatabase(mMainConn, NS_ConvertUTF16toUTF8(iconsPath),
                      NS_LITERAL_CSTRING("favicons"));
  if (NS_FAILED(rv)) {
    // The favicons database may be corrupt. Try to replace and reattach it.
    rv = iconsFile->Remove(true);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = EnsureFaviconsDatabaseFile(aStorage);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AttachDatabase(mMainConn, NS_ConvertUTF16toUTF8(iconsPath),
                        NS_LITERAL_CSTRING("favicons"));
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

nsresult
Database::InitSchema(bool* aDatabaseMigrated)
{
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

  // We are going to update the database, so everything from now on should be in
  // a transaction for performances.
  mozStorageTransaction transaction(mMainConn, false);

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

      if (currentSchemaVersion < 11) {
        // These are versions older than Firefox 4 that are not supported
        // anymore.  In this case it's safer to just replace the database.
        return NS_ERROR_FILE_CORRUPTED;
      }

      // Firefox 4 uses schema version 11.

      // Firefox 8 uses schema version 12.

      if (currentSchemaVersion < 13) {
        rv = MigrateV13Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 15) {
        rv = MigrateV15Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 17) {
        rv = MigrateV17Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 12 uses schema version 17.

      if (currentSchemaVersion < 18) {
        rv = MigrateV18Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 19) {
        rv = MigrateV19Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 13 uses schema version 19.

      if (currentSchemaVersion < 20) {
        rv = MigrateV20Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 21) {
        rv = MigrateV21Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 14 uses schema version 21.

      if (currentSchemaVersion < 22) {
        rv = MigrateV22Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 22 uses schema version 22.

      if (currentSchemaVersion < 23) {
        rv = MigrateV23Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 24 uses schema version 23.

      if (currentSchemaVersion < 24) {
        rv = MigrateV24Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 34 uses schema version 24.

      if (currentSchemaVersion < 25) {
        rv = MigrateV25Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 36 uses schema version 25.

      if (currentSchemaVersion < 26) {
        rv = MigrateV26Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 37 uses schema version 26.

      if (currentSchemaVersion < 27) {
        rv = MigrateV27Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 28) {
        rv = MigrateV28Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 39 uses schema version 28.

      if (currentSchemaVersion < 30) {
        rv = MigrateV30Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 41 uses schema version 30.

      if (currentSchemaVersion < 31) {
        rv = MigrateV31Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 48 uses schema version 31.

      if (currentSchemaVersion < 32) {
        rv = MigrateV32Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 49 uses schema version 32.

      if (currentSchemaVersion < 33) {
        rv = MigrateV33Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 50 uses schema version 33.

      if (currentSchemaVersion < 34) {
        rv = MigrateV34Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 51 uses schema version 34.

      if (currentSchemaVersion < 35) {
        rv = MigrateV35Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 52 uses schema version 35.

      if (currentSchemaVersion < 36) {
        rv = MigrateV36Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 37) {
        rv = MigrateV37Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 55 uses schema version 37.

      // Schema Upgrades must add migration code here.

      rv = UpdateBookmarkRootTitles();
      // We don't want a broken localization to cause us to think
      // the database is corrupt and needs to be replaced.
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
  else {
    // This is a new database, so we have to create all the tables and indices.

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

    // moz_hosts.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_HOSTS);
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
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
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

    // Initialize the bookmark roots in the new DB.
    rv = CreateBookmarkRoots();
    NS_ENSURE_SUCCESS(rv, rv);
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

nsresult
Database::CreateBookmarkRoots()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIStringBundleService> bundleService =
    services::GetStringBundleService();
  NS_ENSURE_STATE(bundleService);
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle(PLACES_BUNDLE, getter_AddRefs(bundle));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLString rootTitle;
  // The first root's title is an empty string.
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("places"),
                  NS_LITERAL_CSTRING("root________"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  // Fetch the internationalized folder name from the string bundle.
  rv = bundle->GetStringFromName(u"BookmarksMenuFolderTitle",
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("menu"),
                  NS_LITERAL_CSTRING("menu________"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(u"BookmarksToolbarFolderTitle",
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("toolbar"),
                  NS_LITERAL_CSTRING("toolbar_____"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(u"TagsFolderTitle",
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("tags"),
                  NS_LITERAL_CSTRING("tags________"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(u"OtherBookmarksFolderTitle",
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("unfiled"),
                  NS_LITERAL_CSTRING("unfiled_____"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  int64_t mobileRootId = CreateMobileRoot();
  if (mobileRootId <= 0) return NS_ERROR_FAILURE;
  {
    nsCOMPtr<mozIStorageStatement> mobileRootSyncStatusStmt;
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks SET syncStatus = :sync_status WHERE id = :id"
    ), getter_AddRefs(mobileRootSyncStatusStmt));
    if (NS_FAILED(rv)) return rv;
    mozStorageStatementScoper mobileRootSyncStatusScoper(
      mobileRootSyncStatusStmt);

    rv = mobileRootSyncStatusStmt->BindInt32ByName(
      NS_LITERAL_CSTRING("sync_status"),
      nsINavBookmarksService::SYNC_STATUS_NEW
    );
    if (NS_FAILED(rv)) return rv;
    rv = mobileRootSyncStatusStmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                                   mobileRootId);
    if (NS_FAILED(rv)) return rv;

    rv = mobileRootSyncStatusStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

#if DEBUG
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT count(*), sum(position) FROM moz_bookmarks"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(hasResult);
  int32_t bookmarkCount = stmt->AsInt32(0);
  int32_t positionSum = stmt->AsInt32(1);
  MOZ_ASSERT(bookmarkCount == 6 && positionSum == 10);
#endif

  return NS_OK;
}

nsresult
Database::InitFunctions()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = GetUnreversedHostFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = MatchAutoCompleteFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CalculateFrecencyFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GenerateGUIDFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = FixupURLFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = FrecencyNotificationFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = StoreLastInsertedIdFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = HashFunction::create(mMainConn);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::InitTempEntities()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the triggers that update the moz_hosts table as necessary.
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_UPDATEHOSTS_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_UPDATEHOSTS_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERUPDATE_TYPED_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_BOOKMARKS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(CREATE_KEYWORDS_FOREIGNCOUNT_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_KEYWORDS_FOREIGNCOUNT_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_KEYWORDS_FOREIGNCOUNT_AFTERUPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::UpdateBookmarkRootTitles()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIStringBundleService> bundleService =
    services::GetStringBundleService();
  NS_ENSURE_STATE(bundleService);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = bundleService->CreateBundle(PLACES_BUNDLE, getter_AddRefs(bundle));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET title = :new_title WHERE guid = :guid"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  rv = stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  if (NS_FAILED(rv)) return rv;

  const char *rootGuids[] = { "menu________"
                            , "toolbar_____"
                            , "tags________"
                            , "unfiled_____"
                            , "mobile______"
                            };
  const char *titleStringIDs[] = { "BookmarksMenuFolderTitle"
                                 , "BookmarksToolbarFolderTitle"
                                 , "TagsFolderTitle"
                                 , "OtherBookmarksFolderTitle"
                                 , "MobileBookmarksFolderTitle"
                                 };

  for (uint32_t i = 0; i < ArrayLength(rootGuids); ++i) {
    nsXPIDLString title;
    rv = bundle->GetStringFromName(NS_ConvertASCIItoUTF16(titleStringIDs[i]).get(),
                                   getter_Copies(title));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<mozIStorageBindingParams> params;
    rv = paramsArray->NewBindingParams(getter_AddRefs(params));
    if (NS_FAILED(rv)) return rv;
    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"),
                                      nsDependentCString(rootGuids[i]));
    if (NS_FAILED(rv)) return rv;
    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("new_title"),
                                      NS_ConvertUTF16toUTF8(title));
    if (NS_FAILED(rv)) return rv;
    rv = paramsArray->AddParams(params);
    if (NS_FAILED(rv)) return rv;
  }

  rv = stmt->BindParameters(paramsArray);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
  rv = stmt->ExecuteAsync(nullptr, getter_AddRefs(pendingStmt));
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult
Database::MigrateV13Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Dynamic containers are no longer supported.
  nsCOMPtr<mozIStorageAsyncStatement> deleteDynContainersStmt;
  nsresult rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_bookmarks WHERE type = :item_type"),
    getter_AddRefs(deleteDynContainersStmt));
  rv = deleteDynContainersStmt->BindInt32ByName(
    NS_LITERAL_CSTRING("item_type"),
    nsINavBookmarksService::TYPE_DYNAMIC_CONTAINER
  );
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = deleteDynContainersStmt->ExecuteAsync(nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV15Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Drop moz_bookmarks_beforedelete_v1_trigger, since it's more expensive than
  // useful.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TRIGGER IF EXISTS moz_bookmarks_beforedelete_v1_trigger"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove any orphan keywords.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_keywords "
    "WHERE NOT EXISTS ( "
      "SELECT id "
      "FROM moz_bookmarks "
      "WHERE keyword_id = moz_keywords.id "
    ")"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV17Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool tableExists = false;

  nsresult rv = mMainConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!tableExists) {
    // For anyone who used in-development versions of this autocomplete,
    // drop the old tables and its indexes.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_hostnames_frecencyindex"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TABLE IF EXISTS moz_hostnames"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    // Add the moz_hosts table so we can get hostnames for URL autocomplete.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_HOSTS);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Fill the moz_hosts table with all the domains in moz_places.
  nsCOMPtr<mozIStorageAsyncStatement> fillHostsStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_hosts (host, frecency) "
        "SELECT fixup_url(get_unreversed_host(h.rev_host)) AS host, "
               "(SELECT MAX(frecency) FROM moz_places "
                "WHERE rev_host = h.rev_host "
                   "OR rev_host = h.rev_host || 'www.' "
               ") AS frecency "
        "FROM moz_places h "
        "WHERE LENGTH(h.rev_host) > 1 "
        "GROUP BY h.rev_host"
  ), getter_AddRefs(fillHostsStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = fillHostsStmt->ExecuteAsync(nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV18Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // moz_hosts should distinguish on typed entries.

  // Check if the profile already has a typed column.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT typed FROM moz_hosts"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_hosts ADD COLUMN typed NOT NULL DEFAULT 0"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // With the addition of the typed column the covering index loses its
  // advantages.  On the other side querying on host and (optionally) typed
  // largely restricts the number of results, making scans decently fast.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX IF EXISTS moz_hosts_frecencyhostindex"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Update typed data.
  nsCOMPtr<mozIStorageAsyncStatement> updateTypedStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts SET typed = 1 WHERE host IN ( "
      "SELECT fixup_url(get_unreversed_host(rev_host)) "
      "FROM moz_places WHERE typed = 1 "
    ") "
  ), getter_AddRefs(updateTypedStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = updateTypedStmt->ExecuteAsync(nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV19Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Livemarks children are no longer bookmarks.

  // Remove all children of folders annotated as livemarks.
  nsCOMPtr<mozIStorageStatement> deleteLivemarksChildrenStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_bookmarks WHERE parent IN("
      "SELECT b.id FROM moz_bookmarks b "
      "JOIN moz_items_annos a ON a.item_id = b.id "
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
      "WHERE b.type = :item_type AND n.name = :anno_name "
    ")"
  ), getter_AddRefs(deleteLivemarksChildrenStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksChildrenStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_name"), NS_LITERAL_CSTRING(LMANNO_FEEDURI)
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksChildrenStmt->BindInt32ByName(
    NS_LITERAL_CSTRING("item_type"), nsINavBookmarksService::TYPE_FOLDER
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksChildrenStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear obsolete livemark prefs.
  (void)Preferences::ClearUser("browser.bookmarks.livemark_refresh_seconds");
  (void)Preferences::ClearUser("browser.bookmarks.livemark_refresh_limit_count");
  (void)Preferences::ClearUser("browser.bookmarks.livemark_refresh_delay_time");

  // Remove the old status annotations.
  nsCOMPtr<mozIStorageStatement> deleteLivemarksAnnosStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos WHERE anno_attribute_id IN("
      "SELECT id FROM moz_anno_attributes "
      "WHERE name IN (:anno_loading, :anno_loadfailed, :anno_expiration) "
    ")"
  ), getter_AddRefs(deleteLivemarksAnnosStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_loading"), NS_LITERAL_CSTRING("livemark/loading")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_loadfailed"), NS_LITERAL_CSTRING("livemark/loadfailed")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_expiration"), NS_LITERAL_CSTRING("livemark/expiration")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove orphan annotation names.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_anno_attributes "
      "WHERE name IN (:anno_loading, :anno_loadfailed, :anno_expiration) "
  ), getter_AddRefs(deleteLivemarksAnnosStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_loading"), NS_LITERAL_CSTRING("livemark/loading")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_loadfailed"), NS_LITERAL_CSTRING("livemark/loadfailed")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_expiration"), NS_LITERAL_CSTRING("livemark/expiration")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteLivemarksAnnosStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV20Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Remove obsolete bookmark GUID annotations.
  nsCOMPtr<mozIStorageStatement> deleteOldBookmarkGUIDAnnosStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos WHERE anno_attribute_id = ("
      "SELECT id FROM moz_anno_attributes "
      "WHERE name = :anno_guid"
    ")"
  ), getter_AddRefs(deleteOldBookmarkGUIDAnnosStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteOldBookmarkGUIDAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_guid"), NS_LITERAL_CSTRING("placesInternal/GUID")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteOldBookmarkGUIDAnnosStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove the orphan annotation name.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_anno_attributes "
      "WHERE name = :anno_guid"
  ), getter_AddRefs(deleteOldBookmarkGUIDAnnosStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteOldBookmarkGUIDAnnosStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_guid"), NS_LITERAL_CSTRING("placesInternal/GUID")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteOldBookmarkGUIDAnnosStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV21Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Add a prefix column to moz_hosts.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT prefix FROM moz_hosts"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_hosts ADD COLUMN prefix"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
Database::MigrateV22Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Reset all session IDs to 0 since we don't support them anymore.
  // We don't set them to NULL to avoid breaking downgrades.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT session FROM moz_historyvisits"
  ), getter_AddRefs(stmt));
  if (NS_SUCCEEDED(rv)) {
    nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE moz_historyvisits SET session = 0"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
Database::MigrateV23Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Recalculate hosts prefixes.
  nsCOMPtr<mozIStorageAsyncStatement> updatePrefixesStmt;
  nsresult rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts SET prefix = ( " HOSTS_PREFIX_PRIORITY_FRAGMENT ") "
  ), getter_AddRefs(updatePrefixesStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = updatePrefixesStmt->ExecuteAsync(nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV24Up()
{
  MOZ_ASSERT(NS_IsMainThread());

 // Add a foreign_count column to moz_places
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT foreign_count FROM moz_places"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_places ADD COLUMN foreign_count INTEGER DEFAULT 0 NOT NULL"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Adjust counts for all the rows
  nsCOMPtr<mozIStorageStatement> updateStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET foreign_count = "
    "(SELECT count(*) FROM moz_bookmarks WHERE fk = moz_places.id) "
  ), getter_AddRefs(updateStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper updateScoper(updateStmt);
  rv = updateStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV25Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Change bookmark roots GUIDs to constant values.

  // If moz_bookmarks_roots doesn't exist anymore, it's because we finally have
  // been able to remove it.  In such a case, we already assigned constant GUIDs
  // to the roots and we can skip this migration.
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT root_name FROM moz_bookmarks_roots"
    ), getter_AddRefs(stmt));
    if (NS_FAILED(rv)) {
      return NS_OK;
    }
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET guid = :guid "
    "WHERE id = (SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = :name) "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  const char *rootNames[] = { "places", "menu", "toolbar", "tags", "unfiled" };
  const char *rootGuids[] = { "root________"
                            , "menu________"
                            , "toolbar_____"
                            , "tags________"
                            , "unfiled_____"
                            };

  for (uint32_t i = 0; i < ArrayLength(rootNames); ++i) {
    // Since this is using the synchronous API, we cannot use
    // a BindingParamsArray.
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                      nsDependentCString(rootNames[i]));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"),
                                      nsDependentCString(rootGuids[i]));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
Database::MigrateV26Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // Round down dateAdded and lastModified values to milliseconds precision.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET dateAdded = dateAdded - dateAdded % 1000, "
    "                         lastModified = lastModified - lastModified % 1000"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV27Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // Change keywords store, moving their relation from bookmarks to urls.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT place_id FROM moz_keywords"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    // Even if these 2 columns have a unique constraint, we allow NULL values
    // for backwards compatibility. NULL never breaks a unique constraint.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_keywords ADD COLUMN place_id INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_keywords ADD COLUMN post_data TEXT"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_KEYWORDS_PLACEPOSTDATA);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Associate keywords with uris.  A keyword could be associated to multiple
  // bookmarks uris, or multiple keywords could be associated to the same uri.
  // The new system only allows multiple uris per keyword, provided they have
  // a different post_data value.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR REPLACE INTO moz_keywords (id, keyword, place_id, post_data) "
    "SELECT k.id, k.keyword, h.id, MAX(a.content) "
    "FROM moz_places h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "JOIN moz_keywords k ON k.id = b.keyword_id "
    "LEFT JOIN moz_items_annos a ON a.item_id = b.id "
                               "AND a.anno_attribute_id = (SELECT id FROM moz_anno_attributes "
                                                          "WHERE name = 'bookmarkProperties/POSTData') "
    "WHERE k.place_id ISNULL "
    "GROUP BY keyword"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove any keyword that points to a non-existing place id.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_keywords "
    "WHERE NOT EXISTS (SELECT 1 FROM moz_places WHERE id = moz_keywords.place_id)"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET keyword_id = NULL "
    "WHERE NOT EXISTS (SELECT 1 FROM moz_keywords WHERE id = moz_bookmarks.keyword_id)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Adjust foreign_count for all the rows.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET foreign_count = "
    "(SELECT count(*) FROM moz_bookmarks WHERE fk = moz_places.id) + "
    "(SELECT count(*) FROM moz_keywords WHERE place_id = moz_places.id) "
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV28Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // v27 migration was bogus and set some unrelated annotations as post_data for
  // keywords having an annotated bookmark.
  // The current v27 migration function is fixed, but we still need to handle
  // users that hit the bogus version.  Since we can't distinguish, we'll just
  // set again all of the post data.
  DebugOnly<nsresult> rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_keywords "
    "SET post_data = ( "
      "SELECT content FROM moz_items_annos a "
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
      "JOIN moz_bookmarks b on b.id = a.item_id "
      "WHERE n.name = 'bookmarkProperties/POSTData' "
      "AND b.keyword_id = moz_keywords.id "
      "ORDER BY b.lastModified DESC "
      "LIMIT 1 "
    ") "
    "WHERE EXISTS(SELECT 1 FROM moz_bookmarks WHERE keyword_id = moz_keywords.id) "
  ));
  // In case the update fails a constraint, we don't want to throw away the
  // whole database for just a few keywords.  In rare cases the user might have
  // to recreate them.  Though, at this point, there shouldn't be 2 keywords
  // pointing to the same url and post data, cause the previous migration step
  // removed them.
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
Database::MigrateV30Up() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX IF EXISTS moz_favicons_guid_uniqueindex"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV31Up() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE IF EXISTS moz_bookmarks_roots"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV32Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // Remove some old and no more used Places preferences that may be confusing
  // for the user.
  mozilla::Unused << Preferences::ClearUser("places.history.expiration.transient_optimal_database_size");
  mozilla::Unused << Preferences::ClearUser("places.last_vacuum");
  mozilla::Unused << Preferences::ClearUser("browser.history_expire_sites");
  mozilla::Unused << Preferences::ClearUser("browser.history_expire_days.mirror");
  mozilla::Unused << Preferences::ClearUser("browser.history_expire_days_min");

  // For performance reasons we want to remove too long urls from history.
  // We cannot use the moz_places triggers here, cause they are defined only
  // after the schema migration.  Thus we need to collect the hosts that need to
  // be updated first.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TEMP TABLE moz_migrate_v32_temp ("
      "host TEXT PRIMARY KEY "
    ") WITHOUT ROWID "
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT OR IGNORE INTO moz_migrate_v32_temp (host) "
        "SELECT fixup_url(get_unreversed_host(rev_host)) "
        "FROM moz_places WHERE LENGTH(url) > :maxlen AND foreign_count = 0"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("maxlen"), MaxUrlLength());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Now remove the pages with a long url.
  {
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places WHERE LENGTH(url) > :maxlen AND foreign_count = 0"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("maxlen"), MaxUrlLength());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Expire orphan visits and update moz_hosts.
  // These may be a bit more expensive and are not critical for the DB
  // functionality, so we execute them asynchronously.
  nsCOMPtr<mozIStorageAsyncStatement> expireOrphansStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_historyvisits "
    "WHERE NOT EXISTS (SELECT 1 FROM moz_places WHERE id = place_id)"
  ), getter_AddRefs(expireOrphansStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageAsyncStatement> deleteHostsStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_hosts "
    "WHERE host IN (SELECT host FROM moz_migrate_v32_temp) "
      "AND NOT EXISTS("
        "SELECT 1 FROM moz_places "
          "WHERE rev_host = get_unreversed_host(host || '.') || '.' "
             "OR rev_host = get_unreversed_host(host || '.') || '.www.' "
      "); "
  ), getter_AddRefs(deleteHostsStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageAsyncStatement> updateHostsStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts "
    "SET prefix = (" HOSTS_PREFIX_PRIORITY_FRAGMENT ") "
    "WHERE host IN (SELECT host FROM moz_migrate_v32_temp) "
  ), getter_AddRefs(updateHostsStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageAsyncStatement> dropTableStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DROP TABLE IF EXISTS moz_migrate_v32_temp"
  ), getter_AddRefs(dropTableStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  mozIStorageBaseStatement *stmts[] = {
    expireOrphansStmt,
    deleteHostsStmt,
    updateHostsStmt,
    dropTableStmt
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = mMainConn->ExecuteAsync(stmts, ArrayLength(stmts), nullptr,
                               getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV33Up() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP INDEX IF EXISTS moz_places_url_uniqueindex"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add an url_hash column to moz_places.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT url_hash FROM moz_places"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_places ADD COLUMN url_hash INTEGER DEFAULT 0 NOT NULL"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET url_hash = hash(url) WHERE url_hash = 0"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create an index on url_hash.
  rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_URL_HASH);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV34Up() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_keywords WHERE id IN ( "
      "SELECT id FROM moz_keywords k "
      "WHERE NOT EXISTS (SELECT 1 FROM moz_places h WHERE k.place_id = h.id) "
    ")"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV35Up() {
  MOZ_ASSERT(NS_IsMainThread());

  int64_t mobileRootId = CreateMobileRoot();
  if (mobileRootId <= 0)  {
    // Either the schema is broken or there isn't any root. The latter can
    // happen if a consumer, for example Thunderbird, never used bookmarks.
    // If there are no roots, this migration should not run.
    nsCOMPtr<mozIStorageStatement> checkRootsStmt;
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_bookmarks WHERE parent = 0"
    ), getter_AddRefs(checkRootsStmt));
    NS_ENSURE_SUCCESS(rv, rv);
    mozStorageStatementScoper scoper(checkRootsStmt);
    bool hasResult = false;
    rv = checkRootsStmt->ExecuteStep(&hasResult);
    if (NS_SUCCEEDED(rv) && !hasResult) {
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  // At this point, we should have no more than two folders with the mobile
  // bookmarks anno: the new root, and the old folder if one exists. If, for
  // some reason, we have multiple folders with the anno, we append their
  // children to the new root.
  nsTArray<int64_t> folderIds;
  nsresult rv = GetItemsWithAnno(NS_LITERAL_CSTRING(MOBILE_ROOT_ANNO),
                                 nsINavBookmarksService::TYPE_FOLDER,
                                 folderIds);
  if (NS_FAILED(rv)) return rv;

  for (uint32_t i = 0; i < folderIds.Length(); ++i) {
    if (folderIds[i] == mobileRootId) {
      // Ignore the new mobile root. We'll remove this anno from the root in
      // bug 1306445.
      continue;
    }

    // Append the folder's children to the new root.
    nsCOMPtr<mozIStorageStatement> moveStmt;
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks "
      "SET parent = :root_id, "
          "position = position + IFNULL("
            "(SELECT MAX(position) + 1 FROM moz_bookmarks "
             "WHERE parent = :root_id), 0)"
      "WHERE parent = :folder_id"
    ), getter_AddRefs(moveStmt));
    if (NS_FAILED(rv)) return rv;
    mozStorageStatementScoper moveScoper(moveStmt);

    rv = moveStmt->BindInt64ByName(NS_LITERAL_CSTRING("root_id"),
                                   mobileRootId);
    if (NS_FAILED(rv)) return rv;
    rv = moveStmt->BindInt64ByName(NS_LITERAL_CSTRING("folder_id"),
                                   folderIds[i]);
    if (NS_FAILED(rv)) return rv;

    rv = moveStmt->Execute();
    if (NS_FAILED(rv)) return rv;

    // Delete the old folder.
    rv = DeleteBookmarkItem(folderIds[i]);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

nsresult
Database::MigrateV36Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // Add sync status and change counter tracking columns for bookmarks.
  nsCOMPtr<mozIStorageStatement> syncStatusStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT syncStatus FROM moz_bookmarks"
  ), getter_AddRefs(syncStatusStmt));
  if (NS_FAILED(rv)) {
    // We default to SYNC_STATUS_UNKNOWN = 0 for existing bookmarks, matching
    // the bookmark restore behavior. If Sync is set up, we'll update the status
    // to SYNC_STATUS_NORMAL = 2 before the first post-migration sync.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_bookmarks "
      "ADD COLUMN syncStatus INTEGER DEFAULT 0 NOT NULL"
    ));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<mozIStorageStatement> syncChangeCounterStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT syncChangeCounter FROM moz_bookmarks"
  ), getter_AddRefs(syncChangeCounterStmt));
  if (NS_FAILED(rv)) {
    // The change counter starts at 1 for all local bookmarks. It's incremented
    // for each modification that should trigger a sync, and decremented after
    // the modified bookmark is uploaded to the server.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_bookmarks "
      "ADD COLUMN syncChangeCounter INTEGER DEFAULT 1 NOT NULL"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<mozIStorageStatement> tombstoneTableStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT 1 FROM moz_bookmarks_deleted"
  ), getter_AddRefs(tombstoneTableStmt));
  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS_DELETED);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
Database::MigrateV37Up() {
  MOZ_ASSERT(NS_IsMainThread());

  // Move favicons to the new database.
  // For now we retain the old moz_favicons table, but we empty it.
  // This allows for a "safer" downgrade, even if icons will be lost in the
  // process. In a couple versions we shall drop moz_favicons completely.

  // First, check if the old favicons table still exists.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT url FROM moz_favicons"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) {
    // The table has already been removed, nothing to do.
    return NS_OK;
  }

  // The new table accepts only png or svg payloads, so we set a valid width
  // only for them, the mime-type for the others.  Later we will asynchronously
  // try to convert the unsupported payloads, or remove them.

  // Add pages.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO moz_pages_w_icons (page_url, page_url_hash) "
    "SELECT h.url, hash(h.url) "
    "FROM moz_places h "
    "JOIN moz_favicons f ON f.id = h.favicon_id"
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  // Set icons as expired, so we will replace them with proper versions at the
  // first load.
  // Note: we use a peculiarity of Sqlite here, where the column affinity
  // is not enforced, thanks to that we can store a string in an integer column.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO moz_icons (icon_url, fixed_icon_url_hash, width, data) "
      "SELECT url, hash(fixup_url(url)), "
             "(CASE WHEN mime_type = 'image/png' THEN 16 "
                   "WHEN mime_type = 'image/svg+xml' THEN 65535 "
                   "ELSE mime_type END), "
             "data FROM moz_favicons "
             "WHERE LENGTH(data) > 0 "
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  // Create relations.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_icons_to_pages (page_id, icon_id) "
      "SELECT (SELECT id FROM moz_pages_w_icons "
              "WHERE page_url_hash = h.url_hash "
                "AND page_url = h.url), "
             "(SELECT id FROM moz_icons "
              "WHERE fixed_icon_url_hash = hash(fixup_url(f.url)) "
                "AND icon_url = f.url) "
      "FROM moz_favicons f "
      "JOIN moz_places h on f.id = h.favicon_id"
  ));
  NS_ENSURE_SUCCESS(rv, rv);
  // Remove old favicons and relations.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_favicons"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET favicon_id = NULL"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  // Start the async conversion
  nsFaviconService::ConvertUnsupportedPayloads(mMainConn);

  return NS_OK;
}

nsresult
Database::GetItemsWithAnno(const nsACString& aAnnoName, int32_t aItemType,
                           nsTArray<int64_t>& aItemIds)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT b.id FROM moz_items_annos a "
    "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
    "JOIN moz_bookmarks b ON b.id = a.item_id "
    "WHERE n.name = :anno_name AND "
          "b.type = :item_type"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"), aAnnoName);
  if (NS_FAILED(rv)) return rv;
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_type"), aItemType);
  if (NS_FAILED(rv)) return rv;

  bool hasMore = false;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    int64_t itemId;
    rv = stmt->GetInt64(0, &itemId);
    if (NS_FAILED(rv)) return rv;
    aItemIds.AppendElement(itemId);
  }

  return NS_OK;
}

nsresult
Database::DeleteBookmarkItem(int32_t aItemId)
{
  // Delete the old bookmark.
  nsCOMPtr<mozIStorageStatement> deleteStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_bookmarks WHERE id = :item_id"
  ), getter_AddRefs(deleteStmt));
  if (NS_FAILED(rv)) return rv;
  mozStorageStatementScoper deleteScoper(deleteStmt);

  rv = deleteStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                   aItemId);
  if (NS_FAILED(rv)) return rv;

  rv = deleteStmt->Execute();
  if (NS_FAILED(rv)) return rv;

  // Clean up orphan annotations.
  nsCOMPtr<mozIStorageStatement> removeAnnosStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos WHERE item_id = :item_id"
  ), getter_AddRefs(removeAnnosStmt));
  if (NS_FAILED(rv)) return rv;
  mozStorageStatementScoper removeAnnosScoper(removeAnnosStmt);

  rv = removeAnnosStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                        aItemId);
  if (NS_FAILED(rv)) return rv;

  rv = removeAnnosStmt->Execute();
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

int64_t
Database::CreateMobileRoot()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Create the mobile root, ignoring conflicts if one already exists (for
  // example, if the user downgraded to an earlier release channel).
  nsCOMPtr<mozIStorageStatement> createStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_bookmarks "
      "(type, title, dateAdded, lastModified, guid, position, parent) "
    "SELECT :item_type, :item_title, :timestamp, :timestamp, :guid, "
      "(SELECT COUNT(*) FROM moz_bookmarks p WHERE p.parent = b.id), b.id "
    "FROM moz_bookmarks b WHERE b.parent = 0"
  ), getter_AddRefs(createStmt));
  if (NS_FAILED(rv)) return -1;
  mozStorageStatementScoper createScoper(createStmt);

  rv = createStmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"),
                                   nsINavBookmarksService::TYPE_FOLDER);
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"),
                                        NS_LITERAL_CSTRING(MOBILE_ROOT_TITLE));
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindInt64ByName(NS_LITERAL_CSTRING("timestamp"),
                                   RoundedPRNow());
  if (NS_FAILED(rv)) return -1;
  rv = createStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"),
                                        NS_LITERAL_CSTRING(MOBILE_ROOT_GUID));
  if (NS_FAILED(rv)) return -1;

  rv = createStmt->Execute();
  if (NS_FAILED(rv)) return -1;

  // Find the mobile root ID. We can't use the last inserted ID because the
  // root might already exist, and we ignore on conflict.
  nsCOMPtr<mozIStorageStatement> findIdStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM moz_bookmarks WHERE guid = :guid"
  ), getter_AddRefs(findIdStmt));
  if (NS_FAILED(rv)) return -1;
  mozStorageStatementScoper findIdScoper(findIdStmt);

  rv = findIdStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"),
                                        NS_LITERAL_CSTRING(MOBILE_ROOT_GUID));
  if (NS_FAILED(rv)) return -1;

  bool hasResult = false;
  rv = findIdStmt->ExecuteStep(&hasResult);
  if (NS_FAILED(rv) || !hasResult) return -1;

  int64_t rootId;
  rv = findIdStmt->GetInt64(0, &rootId);
  if (NS_FAILED(rv)) return -1;

  // Set the mobile bookmarks anno on the new root, so that Sync code on an
  // older channel can still find it in case of a downgrade. This can be
  // removed in bug 1306445.
  nsCOMPtr<mozIStorageStatement> addAnnoNameStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_anno_attributes (name) VALUES (:anno_name)"
  ), getter_AddRefs(addAnnoNameStmt));
  if (NS_FAILED(rv)) return -1;
  mozStorageStatementScoper addAnnoNameScoper(addAnnoNameStmt);

  rv = addAnnoNameStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_name"), NS_LITERAL_CSTRING(MOBILE_ROOT_ANNO));
  if (NS_FAILED(rv)) return -1;
  rv = addAnnoNameStmt->Execute();
  if (NS_FAILED(rv)) return -1;

  nsCOMPtr<mozIStorageStatement> addAnnoStmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_items_annos "
      "(id, item_id, anno_attribute_id, content, flags, "
       "expiration, type, dateAdded, lastModified) "
    "SELECT "
      "(SELECT a.id FROM moz_items_annos a "
       "WHERE a.anno_attribute_id = n.id AND "
             "a.item_id = :root_id), "
      ":root_id, n.id, 1, 0, :expiration, :type, :timestamp, :timestamp "
    "FROM moz_anno_attributes n WHERE name = :anno_name"
  ), getter_AddRefs(addAnnoStmt));
  if (NS_FAILED(rv)) return -1;
  mozStorageStatementScoper addAnnoScoper(addAnnoStmt);

  rv = addAnnoStmt->BindInt64ByName(NS_LITERAL_CSTRING("root_id"), rootId);
  if (NS_FAILED(rv)) return -1;
  rv = addAnnoStmt->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_name"), NS_LITERAL_CSTRING(MOBILE_ROOT_ANNO));
  if (NS_FAILED(rv)) return -1;
  rv = addAnnoStmt->BindInt32ByName(NS_LITERAL_CSTRING("expiration"),
                                    nsIAnnotationService::EXPIRE_NEVER);
  if (NS_FAILED(rv)) return -1;
  rv = addAnnoStmt->BindInt32ByName(NS_LITERAL_CSTRING("type"),
                                    nsIAnnotationService::TYPE_INT32);
  if (NS_FAILED(rv)) return -1;
  rv = addAnnoStmt->BindInt32ByName(NS_LITERAL_CSTRING("timestamp"),
                                    RoundedPRNow());
  if (NS_FAILED(rv)) return -1;

  rv = addAnnoStmt->Execute();
  if (NS_FAILED(rv)) return -1;

  return rootId;
}

void
Database::Shutdown(bool aInitSucceeded)
{
  // As the last step in the shutdown path, finalize the database handle.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mClosed);

  // Break cycles with the shutdown blockers.
  mClientsShutdown = nullptr;
  nsCOMPtr<mozIStorageCompletionCallback> connectionShutdown = mConnectionShutdown.forget();

  if (!mMainConn) {
    // The connection has never been initialized. Just mark it as closed.
    mClosed = true;
    (void)connectionShutdown->Complete(NS_OK, nullptr);
    return;
  }

#ifdef DEBUG
  if (aInitSucceeded) {
    bool hasResult;
    nsCOMPtr<mozIStorageStatement> stmt;

    // Sanity check for missing guids.
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT 1 "
      "FROM moz_places "
      "WHERE guid IS NULL "
    ), getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a page without a GUID!");
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT 1 "
      "FROM moz_bookmarks "
      "WHERE guid IS NULL "
    ), getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a bookmark without a GUID!");

    // Sanity check for unrounded dateAdded and lastModified values (bug
    // 1107308).
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT 1 "
        "FROM moz_bookmarks "
        "WHERE dateAdded % 1000 > 0 OR lastModified % 1000 > 0 LIMIT 1"
      ), getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found unrounded dates!");

    // Sanity check url_hash
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT 1 FROM moz_places WHERE url_hash = 0"
    ), getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a place without a hash!");

    // Sanity check unique urls
    rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT 1 FROM moz_places GROUP BY url HAVING count(*) > 1 "
    ), getter_AddRefs(stmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = stmt->ExecuteStep(&hasResult);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(!hasResult, "Found a duplicate url!");
  }
#endif

  mMainThreadStatements.FinalizeStatements();
  mMainThreadAsyncStatements.FinalizeStatements();

  RefPtr< FinalizeStatementCacheProxy<mozIStorageStatement> > event =
    new FinalizeStatementCacheProxy<mozIStorageStatement>(
          mAsyncThreadStatements,
          NS_ISUPPORTS_CAST(nsIObserver*, this)
        );
  DispatchToAsyncThread(event);

  mClosed = true;

  (void)mMainConn->AsyncClose(connectionShutdown);
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
Database::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const char16_t *aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (strcmp(aTopic, TOPIC_PROFILE_CHANGE_TEARDOWN) == 0) {
    // Tests simulating shutdown may cause multiple notifications.
    if (IsShutdownStarted()) {
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
                     getter_AddRefs(e))) && e) {
      bool hasMore = false;
      while (NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> supports;
        if (NS_SUCCEEDED(e->GetNext(getter_AddRefs(supports)))) {
          nsCOMPtr<nsIObserver> observer = do_QueryInterface(supports);
          (void)observer->Observe(observer, TOPIC_PLACES_INIT_COMPLETE, nullptr);
        }
      }
    }

    // Notify all Places users that we are about to shutdown.
    (void)os->NotifyObservers(nullptr, TOPIC_PLACES_SHUTDOWN, nullptr);
  } else if (strcmp(aTopic, TOPIC_SIMULATE_PLACES_SHUTDOWN) == 0) {
    // This notification is (and must be) only used by tests that are trying
    // to simulate Places shutdown out of the normal shutdown path.

    // Tests simulating shutdown may cause re-entrance.
    if (IsShutdownStarted()) {
      return NS_OK;
    }

    // We are simulating a shutdown, so invoke the shutdown blockers,
    // wait for them, then proceed with connection shutdown.
    // Since we are already going through shutdown, but it's not the real one,
    // we won't need to block the real one anymore, so we can unblock it.
    {
      nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetProfileChangeTeardownPhase();
      if (shutdownPhase) {
        shutdownPhase->RemoveBlocker(mClientsShutdown.get());
      }
      (void)mClientsShutdown->BlockShutdown(nullptr);
    }

    // Spin the events loop until the clients are done.
    // Note, this is just for tests, specifically test_clearHistory_shutdown.js
    SpinEventLoopUntil([&]() {
	return mClientsShutdown->State() == PlacesShutdownBlocker::States::RECEIVED_DONE;
      });

    {
      nsCOMPtr<nsIAsyncShutdownClient> shutdownPhase = GetProfileBeforeChangePhase();
      if (shutdownPhase) {
        shutdownPhase->RemoveBlocker(mConnectionShutdown.get());
      }
      (void)mConnectionShutdown->BlockShutdown(nullptr);
    }
  }
  return NS_OK;
}

uint32_t
Database::MaxUrlLength() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMaxUrlLength) {
    mMaxUrlLength = Preferences::GetInt(PREF_HISTORY_MAXURLLEN,
                                        PREF_HISTORY_MAXURLLEN_DEFAULT);
    if (mMaxUrlLength < 255 || mMaxUrlLength > INT32_MAX) {
      mMaxUrlLength = PREF_HISTORY_MAXURLLEN_DEFAULT;
    }
  }
  return mMaxUrlLength;
}



} // namespace places
} // namespace mozilla
