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
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Original contributor(s) of code moved from nsNavHistory.cpp:
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Seth Spitzer <sspitzer@mozilla.com>
 *   Asaf Romano <mano@mozilla.com>
 *   Marco Bonardo <mak77@bonardo.net>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Drew Willcoxon <adw@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Paolo Amadini <http://www.amadzone.org/>
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

#include "Database.h"

#include "nsINavBookmarksService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILocalFile.h"

#include "nsNavHistory.h"
#include "nsPlacesTables.h"
#include "nsPlacesIndexes.h"
#include "nsPlacesTriggers.h"
#include "nsPlacesMacros.h"
#include "SQLFunctions.h"
#include "Helpers.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "prsystem.h"
#include "nsPrintfCString.h"
#include "mozilla/Util.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

// Time between corrupt database backups.
#define RECENT_BACKUP_TIME_MICROSEC (PRInt64)86400 * PR_USEC_PER_SEC // 24H

// Filename of the database.
#define DATABASE_FILENAME NS_LITERAL_STRING("places.sqlite")
// Filename used to backup corrupt databases.
#define DATABASE_CORRUPT_FILENAME NS_LITERAL_STRING("places.sqlite.corrupt")

// Set when the database file was found corrupt by a previous maintenance.
#define PREF_FORCE_DATABASE_REPLACEMENT "places.database.replaceOnStartup"
// Set to the calculated optimal size of the database.  This is a target size
// used to evaluate history limits.  It's not mandatory.
#define PREF_OPTIMAL_DATABASE_SIZE "places.history.expiration.transient_optimal_database_size"

// To calculate the cache size we take into account the available physical
// memory and the current database size.  This is the percentage of memory
// we reserve for the former case.
#define DATABASE_CACHE_TO_MEMORY_PERC 2
// The minimum size of the cache.  We should never work without a cache, since
// that would badly hurt WAL journaling mode.
#define DATABASE_CACHE_MIN_BYTES (PRUint64)5242880 // 5MiB
// We calculate an optimal database size, based on hardware specs.  This
// pertains more to expiration, but the code is pretty much the same used for
// cache_size, so it's here to reduce code duplication.
// This percentage of disk size is used to protect against calculating a too
// large size on disks with tiny quota or available space.
#define DATABASE_TO_DISK_PERC 2
// Maximum size of the optimal database.  High-end hardware has plenty of
// memory and disk space, but performances don't grow linearly.
#define DATABASE_MAX_SIZE (PRInt64)167772160 // 160MiB
// If the physical memory size is not available, use MEMSIZE_FALLBACK_BYTES
// instead.  Must stay in sync with the code in nsPlacesExpiration.js.
#define MEMSIZE_FALLBACK_BYTES 268435456 // 256 M

// Maximum size for the WAL file.  It should be small enough since in case of
// crashes we could lose all the transactions in the file.  But a too small
// file could hurt performance.
#define DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES 512

#define BYTES_PER_MEBIBYTE 1048576

// Old Sync GUID annotation.
#define SYNCGUID_ANNO NS_LITERAL_CSTRING("sync/guid")

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
      PRInt64 lastMod = 0;
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
  (void)aDBConn->ExecuteAsync(stmts, ArrayLength(stmts), nsnull,
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
  nsCAutoString journalMode;
  switch (aJournalMode) {
    default:
      MOZ_ASSERT("Trying to set an unknown journal mode.");
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
  nsCAutoString query("PRAGMA journal_mode = ");
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
    // This is an unknown journal.
    MOZ_ASSERT(true);
  }

  return JOURNAL_DELETE;
}

} // Anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// Database

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(Database, gDatabase)

NS_IMPL_THREADSAFE_ISUPPORTS2(Database
, nsIObserver
, nsISupportsWeakReference
)

Database::Database()
  : mMainThreadStatements(mMainConn)
  , mMainThreadAsyncStatements(mMainConn)
  , mAsyncThreadStatements(mMainConn)
  , mDBPageSize(0)
  , mCurrentJournalMode(JOURNAL_DELETE)
  , mDatabaseStatus(nsINavHistoryService::DATABASE_STATUS_OK)
  , mShuttingDown(false)
{
  // Attempting to create two instances of the service?
  MOZ_ASSERT(!gDatabase);
  gDatabase = this;
}

Database::~Database()
{
  // Check to make sure it's us, in case somebody wrongly creates an extra
  // instance of this singleton class.
  MOZ_ASSERT(gDatabase == this);

  // Remove the static reference to the service.
  if (gDatabase == this) {
    gDatabase = nsnull;
  }
}

nsresult
Database::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

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
    nsRefPtr<PlacesEvent> lockedEvent = new PlacesEvent(TOPIC_DATABASE_LOCKED);
    (void)NS_DispatchToMainThread(lockedEvent);
    return rv;
  }

  // Initialize the database schema.  In case of failure the existing schema is
  // is corrupt or incoherent, thus the database should be replaced.
  bool databaseMigrated = false;
  rv = InitSchema(&databaseMigrated);
  if (NS_FAILED(rv)) {
    mDatabaseStatus = nsINavHistoryService::DATABASE_STATUS_CORRUPT;
    rv = BackupAndReplaceDatabaseFile(storage);
    NS_ENSURE_SUCCESS(rv, rv);
    // Try to initialize the schema again on the new database.
    rv = InitSchema(&databaseMigrated);
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

  rv = InitTempTriggers();
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify we have finished database initialization.
  // Enqueue the notification, so if we init another service that requires
  // nsNavHistoryService we don't recursive try to get it.
  nsRefPtr<PlacesEvent> completeEvent =
    new PlacesEvent(TOPIC_PLACES_INIT_COMPLETE);
  rv = NS_DispatchToMainThread(completeEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally observe profile shutdown notifications.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->AddObserver(this, TOPIC_PROFILE_CHANGE_TEARDOWN, true);
    (void)os->AddObserver(this, TOPIC_PROFILE_BEFORE_CHANGE, true);
  }

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

  // Close database connection if open.
  if (mMainConn) {
    // If there's any not finalized statement or this fails for any reason
    // we won't be able to remove the database.
    rv = mMainConn->Close();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Remove the broken database.
  rv = databaseFile->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a new database file.
  // Use an unshared connection, it will consume more memory but avoid shared
  // cache contentions across threads.
  rv = aStorage->OpenUnsharedDatabase(databaseFile, getter_AddRefs(mMainConn));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::InitSchema(bool* aDatabaseMigrated)
{
  MOZ_ASSERT(NS_IsMainThread());
  *aDatabaseMigrated = false;

  // WARNING: any statement executed before setting the journal mode must be
  // finalized, since SQLite doesn't allow changing the journal mode if there
  // is any outstanding statement.

  {
    // Get the page size.  This may be different than the default if the
    // database file already existed with a different page size.
    nsCOMPtr<mozIStorageStatement> statement;
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "PRAGMA page_size"
    ), getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    bool hasResult = false;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FAILURE);
    rv = statement->GetInt32(0, &mDBPageSize);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && mDBPageSize > 0, NS_ERROR_UNEXPECTED);
  }

  // Ensure that temp tables are held in memory, not on disk.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA temp_store = MEMORY"));
  NS_ENSURE_SUCCESS(rv, rv);

  // We want to work with a cache that is at a maximum half of the database
  // size.  We also want it to respect the available memory size.

  // Calculate memory size, fallback to a meaningful value if it fails.
  PRUint64 memSizeBytes = PR_GetPhysicalMemorySize();
  if (memSizeBytes == 0) {
    memSizeBytes = MEMSIZE_FALLBACK_BYTES;
  }

  PRUint64 cacheSize = memSizeBytes * DATABASE_CACHE_TO_MEMORY_PERC / 100;

  // Calculate an optimal database size for expiration purposes.
  // We usually want to work with a cache that is half the database size.
  // Limit the size to avoid extreme values on high-end hardware.
  PRInt64 optimalDatabaseSize = NS_MIN(static_cast<PRInt64>(cacheSize) * 2,
                                       DATABASE_MAX_SIZE);

  // Protect against a full disk or tiny quota.
  PRInt64 diskAvailableBytes = 0;
  nsCOMPtr<nsIFile> databaseFile;
  mMainConn->GetDatabaseFile(getter_AddRefs(databaseFile));
  NS_ENSURE_STATE(databaseFile);
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(databaseFile);
  if (localFile &&
      NS_SUCCEEDED(localFile->GetDiskSpaceAvailable(&diskAvailableBytes)) &&
      diskAvailableBytes > 0) {
    optimalDatabaseSize = NS_MIN(optimalDatabaseSize,
                                 diskAvailableBytes * DATABASE_TO_DISK_PERC / 100);
  }

  // Share the calculated size if it's meaningful.
  if (optimalDatabaseSize < PR_INT32_MAX) {
    (void)Preferences::SetInt(PREF_OPTIMAL_DATABASE_SIZE,
                              static_cast<PRInt32>(optimalDatabaseSize));
  }

  // Get the current database size. Due to chunked growth we have to use
  // page_count to evaluate it.
  PRUint64 databaseSizeBytes = 0;
  {
    nsCOMPtr<mozIStorageStatement> statement;
    nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "PRAGMA page_count"
    ), getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    bool hasResult = false;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FAILURE);
    PRInt32 pageCount = 0;
    rv = statement->GetInt32(0, &pageCount);
    NS_ENSURE_SUCCESS(rv, rv);
    databaseSizeBytes = pageCount * mDBPageSize;
  }

  // Set cache to a maximum of half the database size.
  cacheSize = NS_MIN(cacheSize, databaseSizeBytes / 2);
  // Ensure we never work without a minimum cache.
  cacheSize = NS_MAX(cacheSize, DATABASE_CACHE_MIN_BYTES);

  // Set the number of cached pages.
  // We don't use PRAGMA default_cache_size, since the database could be moved
  // among different devices and the value would adapt accordingly.
  nsCAutoString cacheSizePragma("PRAGMA cache_size = ");
  cacheSizePragma.AppendInt(cacheSize / mDBPageSize);
  rv = mMainConn->ExecuteSimpleSQL(cacheSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // Be sure to set journal mode after page_size.  WAL would prevent the change
  // otherwise.
  if (NS_SUCCEEDED(SetJournalMode(mMainConn, JOURNAL_WAL))) {
    // Set the WAL journal size limit.  We want it to be small, since in
    // synchronous = NORMAL mode a crash could cause loss of all the
    // transactions in the journal.  For added safety we will also force
    // checkpointing at strategic moments.
    PRInt32 checkpointPages =
      static_cast<PRInt32>(DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES * 1024 / mDBPageSize);
    nsCAutoString checkpointPragma("PRAGMA wal_autocheckpoint = ");
    checkpointPragma.AppendInt(checkpointPages);
    rv = mMainConn->ExecuteSimpleSQL(checkpointPragma);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Ignore errors, if we fail here the database could be considered corrupt
    // and we won't be able to go on, even if it's just matter of a bogus file
    // system.  The default mode (DELETE) will be fine in such a case.
    (void)SetJournalMode(mMainConn, JOURNAL_TRUNCATE);

    // Set synchronous to FULL to ensure maximum data integrity, even in
    // case of crashes or unclean shutdowns.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA synchronous = FULL"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The journal is usually free to grow for performance reasons, but it never
  // shrinks back.  Since the space taken may be problematic, especially on
  // mobile devices, limit its size.
  // Since exceeding the limit will cause a truncate, allow a slightly
  // larger limit than DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES to reduce the number
  // of times it is needed.
  nsCAutoString journalSizePragma("PRAGMA journal_size_limit = ");
  journalSizePragma.AppendInt(DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES * 3);
  (void)mMainConn->ExecuteSimpleSQL(journalSizePragma);

  // Grow places in 10MiB increments to limit fragmentation on disk.
  (void)mMainConn->SetGrowthIncrement(10 * BYTES_PER_MEBIBYTE, EmptyCString());

  // We use our functions during migration, so initialize them now.
  rv = InitFunctions();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the database schema version.
  PRInt32 currentSchemaVersion;
  rv = mMainConn->GetSchemaVersion(&currentSchemaVersion);
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

      // Firefox 3.0 uses schema version 6.

      if (currentSchemaVersion < 7) {
        rv = MigrateV7Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 8) {
        rv = MigrateV8Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 3.5 uses schema version 8.

      if (currentSchemaVersion < 9) {
        rv = MigrateV9Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 10) {
        rv = MigrateV10Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 3.6 uses schema version 10.

      if (currentSchemaVersion < 11) {
        rv = MigrateV11Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 4 uses schema version 11.

      // Firefox 8 uses schema version 12.

      // Schema Upgrades must add migration code here.
    }
  }
  else {
    // This is a new database, so we have to create all the tables and indices.

    // moz_places.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_URL);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FAVICON);
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

    // moz_bookmarks.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACETYPE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PARENTPOSITION);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_bookmarks_roots.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS_ROOTS);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_keywords.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_KEYWORDS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mMainConn->ExecuteSimpleSQL(CREATE_KEYWORD_VALIDITY_TRIGGER);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_favicons.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_FAVICONS);
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
  }

  // Set the schema version to the current one.
  rv = mMainConn->SetSchemaVersion(DATABASE_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ForceWALCheckpoint();

  // ANY FAILURE IN THIS METHOD WILL CAUSE US TO MARK THE DATABASE AS CORRUPT
  // AND TRY TO REPLACE IT.
  // DO NOT PUT HERE ANYTHING THAT IS NOT RELATED TO INITIALIZATION OR MODIFYING
  // THE DISK DATABASE.

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

  return NS_OK;
}

nsresult
Database::InitTempTriggers()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::CheckAndUpdateGUIDs()
{
  MOZ_ASSERT(NS_IsMainThread());

  // First, import any bookmark guids already set by Sync.
  nsCOMPtr<mozIStorageStatement> updateStmt;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks "
    "SET guid = :guid "
    "WHERE id = :item_id "
  ), getter_AddRefs(updateStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT item_id, content "
    "FROM moz_items_annos "
    "JOIN moz_anno_attributes "
    "WHERE name = :anno_name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 itemId;
    rv = stmt->GetInt64(0, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString guid;
    rv = stmt->GetUTF8String(1, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we have an invalid guid, we don't need to do any more work.
    if (!IsValidGUID(guid)) {
      continue;
    }

    mozStorageStatementScoper updateScoper(updateStmt);
    rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      // We just tried to insert a duplicate guid.  Ignore this error, and we
      // will generate a new one next.
      continue;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now, remove all the bookmark guid annotations that we just imported.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos "
    "WHERE anno_attribute_id = ( "
      "SELECT id "
      "FROM moz_anno_attributes "
      "WHERE name = :anno_name "
    ") "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Next, generate guids for any bookmark that does not already have one.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks "
    "SET guid = GENERATE_GUID() "
    "WHERE guid IS NULL "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, import any history guids already set by Sync.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET guid = :guid "
    "WHERE id = :place_id "
  ), getter_AddRefs(updateStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT place_id, content "
    "FROM moz_annos "
    "JOIN moz_anno_attributes "
    "WHERE name = :anno_name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 placeId;
    rv = stmt->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString guid;
    rv = stmt->GetUTF8String(1, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we have an invalid guid, we don't need to do any more work.
    if (!IsValidGUID(guid)) {
      continue;
    }

    mozStorageStatementScoper updateScoper(updateStmt);
    rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("place_id"), placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      // We just tried to insert a duplicate guid.  Ignore this error, and we
      // will generate a new one next.
      continue;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now, remove all the place guid annotations that we just imported.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_annos "
    "WHERE anno_attribute_id = ( "
      "SELECT id "
      "FROM moz_anno_attributes "
      "WHERE name = :anno_name "
    ") "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, generate guids for any places that do not already have one.
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET guid = GENERATE_GUID() "
    "WHERE guid IS NULL "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV7Up() 
{
  MOZ_ASSERT(NS_IsMainThread());
  mozStorageTransaction transaction(mMainConn, false);

  // We need an index on lastModified to catch quickly last modified bookmark
  // title for tag container's children. This will be useful for sync too.
  bool lastModIndexExists = false;
  nsresult rv = mMainConn->IndexExists(
    NS_LITERAL_CSTRING("moz_bookmarks_itemlastmodifiedindex"),
    &lastModIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!lastModIndexExists) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to do a one-time change of the moz_historyvisits.pageindex
  // to speed up finding last visit date when joinin with moz_places.
  // See bug 392399 for more details.
  bool pageIndexExists = false;
  rv = mMainConn->IndexExists(
    NS_LITERAL_CSTRING("moz_historyvisits_pageindex"), &pageIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (pageIndexExists) {
    // drop old index
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_historyvisits_pageindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create the new multi-column index
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // for existing profiles, we may not have a frecency column
  nsCOMPtr<mozIStorageStatement> hasFrecencyStatement;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT frecency FROM moz_places"),
    getter_AddRefs(hasFrecencyStatement));

  if (NS_FAILED(rv)) {
    // Add frecency column to moz_places, default to -1 so that all the
    // frecencies are invalid
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_places ADD frecency INTEGER DEFAULT -1 NOT NULL"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create index for the frecency column
    // XXX multi column index with typed, and visit_count?
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FRECENCY);
    NS_ENSURE_SUCCESS(rv, rv);

    // Invalidate all frecencies, since they need recalculation.
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->invalidateFrecencies(EmptyCString());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Temporary migration code for bug 396300
  nsCOMPtr<mozIStorageStatement> moveUnfiledBookmarks;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks "
      "SET parent = ("
        "SELECT folder_id "
        "FROM moz_bookmarks_roots "
        "WHERE root_name = :root_name "
      ") "
      "WHERE type = :item_type "
      "AND parent = ("
        "SELECT folder_id "
        "FROM moz_bookmarks_roots "
        "WHERE root_name = :parent_name "
      ")"),
    getter_AddRefs(moveUnfiledBookmarks));
  rv = moveUnfiledBookmarks->BindUTF8StringByName(
    NS_LITERAL_CSTRING("root_name"), NS_LITERAL_CSTRING("unfiled")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindInt32ByName(
    NS_LITERAL_CSTRING("item_type"), nsINavBookmarksService::TYPE_BOOKMARK
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindUTF8StringByName(
    NS_LITERAL_CSTRING("parent_name"), NS_LITERAL_CSTRING("places")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a statement to test for trigger creation
  nsCOMPtr<mozIStorageStatement> triggerDetection;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT name "
      "FROM sqlite_master "
      "WHERE type = 'trigger' "
      "AND name = :trigger_name"),
    getter_AddRefs(triggerDetection));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check for existence
  bool triggerExists;
  rv = triggerDetection->BindUTF8StringByName(
    NS_LITERAL_CSTRING("trigger_name"),
    NS_LITERAL_CSTRING("moz_historyvisits_afterinsert_v1_trigger")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->ExecuteStep(&triggerExists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to create two triggers on moz_historyvists to maintain the
  // accuracy of moz_places.visit_count.  For this to work, we must ensure that
  // all moz_places.visit_count values are correct.
  // See bug 416313 for details.
  if (!triggerExists) {
    // First, we do a one-time reset of all the moz_places.visit_count values.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "UPDATE moz_places SET visit_count = "
          "(SELECT count(*) FROM moz_historyvisits "
           "WHERE place_id = moz_places.id "
            "AND visit_type NOT IN ") +
              nsPrintfCString("(0,%d,%d,%d) ",
                              nsINavHistoryService::TRANSITION_EMBED,
                              nsINavHistoryService::TRANSITION_FRAMED_LINK,
                              nsINavHistoryService::TRANSITION_DOWNLOAD) +
          NS_LITERAL_CSTRING(")"));
    NS_ENSURE_SUCCESS(rv, rv);

    // We used to create two triggers here, but we no longer need that with
    // schema version eight and greater.  We've removed their creation here as
    // a result.
  }

  // Check for existence
  rv = triggerDetection->BindUTF8StringByName(
    NS_LITERAL_CSTRING("trigger_name"),
    NS_LITERAL_CSTRING("moz_bookmarks_beforedelete_v1_trigger")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->ExecuteStep(&triggerExists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to create one trigger on moz_bookmarks to remove unused keywords.
  // See bug 421180 for details.
  if (!triggerExists) {
    // First, remove any existing dangling keywords
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DELETE FROM moz_keywords "
        "WHERE id IN ("
          "SELECT k.id "
          "FROM moz_keywords k "
          "LEFT OUTER JOIN moz_bookmarks b "
          "ON b.keyword_id = k.id "
          "WHERE b.id IS NULL"
        ")"));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now we create our trigger
    rv = mMainConn->ExecuteSimpleSQL(CREATE_KEYWORD_VALIDITY_TRIGGER);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
Database::MigrateV8Up()
{
  MOZ_ASSERT(NS_IsMainThread());
  mozStorageTransaction transaction(mMainConn, false);

  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TRIGGER IF EXISTS moz_historyvisits_afterinsert_v1_trigger"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TRIGGER IF EXISTS moz_historyvisits_afterdelete_v1_trigger"));
  NS_ENSURE_SUCCESS(rv, rv);


  // bug #381795 - remove unused indexes
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_places_titleindex"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_annos_item_idindex"));
  NS_ENSURE_SUCCESS(rv, rv);


  // Do a one-time re-creation of the moz_annos indexes (bug 415201)
  bool oldIndexExists = false;
  rv = mMainConn->IndexExists(NS_LITERAL_CSTRING("moz_annos_attributesindex"), &oldIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (oldIndexExists) {
    // drop old uri annos index
    rv = mMainConn->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("DROP INDEX moz_annos_attributesindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create new uri annos index
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);

    // drop old item annos index
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_items_annos_attributesindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create new item annos index
    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ITEMSANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
Database::MigrateV9Up()
{
  MOZ_ASSERT(NS_IsMainThread());
  mozStorageTransaction transaction(mMainConn, false);
  // Added in Bug 488966.  The last_visit_date column caches the last
  // visit date, this enhances SELECT performances when we
  // need to sort visits by visit date.
  // The cached value is synced by triggers on every added or removed visit.
  // See nsPlacesTriggers.h for details on the triggers.
  bool oldIndexExists = false;
  nsresult rv = mMainConn->IndexExists(
    NS_LITERAL_CSTRING("moz_places_lastvisitdateindex"), &oldIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!oldIndexExists) {
    // Add last_visit_date column to moz_places.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_places ADD last_visit_date INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_LASTVISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now let's sync the column contents with real visit dates.
    // This query can be really slow due to disk access, since it will basically
    // dupe the table contents in the journal file, and then write them down
    // in the database.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "UPDATE moz_places SET last_visit_date = "
          "(SELECT MAX(visit_date) "
           "FROM moz_historyvisits "
           "WHERE place_id = moz_places.id)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
Database::MigrateV10Up()
{
  MOZ_ASSERT(NS_IsMainThread());
  // LastModified is set to the same value as dateAdded on item creation.
  // This way we can use lastModified index to sort.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks SET lastModified = dateAdded "
      "WHERE lastModified IS NULL"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
Database::MigrateV11Up()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Temp tables are going away.
  // For triggers correctness, every time we pass through this migration
  // step, we must ensure correctness of visit_count values.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET visit_count = "
      "(SELECT count(*) FROM moz_historyvisits "
       "WHERE place_id = moz_places.id "
        "AND visit_type NOT IN ") +
          nsPrintfCString("(0,%d,%d,%d) ",
                          nsINavHistoryService::TRANSITION_EMBED,
                          nsINavHistoryService::TRANSITION_FRAMED_LINK,
                          nsINavHistoryService::TRANSITION_DOWNLOAD) +
      NS_LITERAL_CSTRING(")")
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // For existing profiles, we may not have a moz_bookmarks.guid column
  nsCOMPtr<mozIStorageStatement> hasGuidStatement;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT guid FROM moz_bookmarks"),
    getter_AddRefs(hasGuidStatement));

  if (NS_FAILED(rv)) {
    // moz_bookmarks grew a guid column.  Add the column, but do not populate it
    // with anything just yet.  We will do that soon.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_bookmarks "
      "ADD COLUMN guid TEXT"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_placess grew a guid column.  Add the column, but do not populate it
    // with anything just yet.  We will do that soon.
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_places "
      "ADD COLUMN guid TEXT"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to update our guids before we do any real database work.
  rv = CheckAndUpdateGUIDs();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
Database::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mShuttingDown);

  mMainThreadStatements.FinalizeStatements();
  mMainThreadAsyncStatements.FinalizeStatements();

  nsRefPtr< FinalizeStatementCacheProxy<mozIStorageStatement> > event =
    new FinalizeStatementCacheProxy<mozIStorageStatement>(
          mAsyncThreadStatements, NS_ISUPPORTS_CAST(nsIObserver*, this)
        );
  DispatchToAsyncThread(event);

  nsRefPtr<PlacesEvent> closeListener =
    new PlacesEvent(TOPIC_PLACES_CONNECTION_CLOSED);
  (void)mMainConn->AsyncClose(closeListener);

  // Don't set this earlier, otherwise some internal helper used on shutdown
  // may bail out.
  mShuttingDown = true;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
Database::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const PRUnichar *aData)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
 
  if (strcmp(aTopic, TOPIC_PROFILE_CHANGE_TEARDOWN) == 0) {
    // Tests simulating shutdown may cause multiple notifications.
    if (mShuttingDown) {
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
        nsCOMPtr<nsIObserver> observer;
        if (NS_SUCCEEDED(e->GetNext(getter_AddRefs(observer)))) {
          (void)observer->Observe(observer, TOPIC_PLACES_INIT_COMPLETE, nsnull);
        }
      }
    }

    // Notify all Places users that we are about to shutdown.
    (void)os->NotifyObservers(nsnull, TOPIC_PLACES_SHUTDOWN, nsnull);
  }

  else if (strcmp(aTopic, TOPIC_PROFILE_BEFORE_CHANGE) == 0) {
    // Tests simulating shutdown may cause re-entrance.
    if (mShuttingDown) {
      return NS_OK;
    }

    // Fire internal shutdown notifications.
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      (void)os->NotifyObservers(nsnull, TOPIC_PLACES_WILL_CLOSE_CONNECTION, nsnull);
    }

#ifdef DEBUG
    { // Sanity check for missing guids.
      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT 1 "
        "FROM moz_places "
        "WHERE guid IS NULL "
        "UNION ALL "
        "SELECT 1 "
        "FROM moz_bookmarks "
        "WHERE guid IS NULL "
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);

      bool haveNullGuids;
      rv = stmt->ExecuteStep(&haveNullGuids);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(!haveNullGuids,
                   "Someone added an entry without adding a GUID!");
    }
#endif

    // As the last step in the shutdown path, finalize the database handle.
    Shutdown();
  }

  return NS_OK;
}

} // namespace places
} // namespace mozilla
