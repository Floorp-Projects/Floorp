/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Database.h"

#include "nsINavBookmarksService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFile.h"

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
#include "mozilla/Attributes.h"

// Time between corrupt database backups.
#define RECENT_BACKUP_TIME_MICROSEC (PRInt64)86400 * PR_USEC_PER_SEC // 24H

// Filename of the database.
#define DATABASE_FILENAME NS_LITERAL_STRING("places.sqlite")
// Filename used to backup corrupt databases.
#define DATABASE_CORRUPT_FILENAME NS_LITERAL_STRING("places.sqlite.corrupt")

// Set when the database file was found corrupt by a previous maintenance.
#define PREF_FORCE_DATABASE_REPLACEMENT "places.database.replaceOnStartup"

// Maximum size for the WAL file.  It should be small enough since in case of
// crashes we could lose all the transactions in the file.  But a too small
// file could hurt performance.
#define DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES 512

#define BYTES_PER_MEBIBYTE 1048576

// Old Sync GUID annotation.
#define SYNCGUID_ANNO NS_LITERAL_CSTRING("sync/guid")

// Places string bundle, contains internationalized bookmark root names.
#define PLACES_BUNDLE "chrome://places/locale/places.properties"

// Livemarks annotations.
#define LMANNO_FEEDURI "livemark/feedURI"
#define LMANNO_SITEURI "livemark/siteURI"

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
  nsCAutoString query(MOZ_STORAGE_UNIQUIFY_QUERY_STR
		      "PRAGMA journal_mode = ");
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

class BlockingConnectionCloseCallback MOZ_FINAL : public mozIStorageCompletionCallback {
  bool mDone;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK
  BlockingConnectionCloseCallback();
  void Spin();
};

NS_IMETHODIMP
BlockingConnectionCloseCallback::Complete()
{
  mDone = true;
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_ASSERT(os);
  if (!os)
    return NS_OK;
  DebugOnly<nsresult> rv = os->NotifyObservers(nsnull,
                                               TOPIC_PLACES_CONNECTION_CLOSED,
                                               nsnull);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return NS_OK;
}

BlockingConnectionCloseCallback::BlockingConnectionCloseCallback()
  : mDone(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

void BlockingConnectionCloseCallback::Spin() {
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  while (!mDone) {
    NS_ProcessNextEvent(thread);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
  BlockingConnectionCloseCallback
, mozIStorageCompletionCallback
)

nsresult
CreateRoot(nsCOMPtr<mozIStorageConnection>& aDBConn,
           const nsCString& aRootName,
           const nsXPIDLString& titleString)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The position of the new item in its folder.
  static PRInt32 itemPosition = 0;

  // A single creation timestamp for all roots so that the root folder's
  // last modification time isn't earlier than its childrens' creation time.
  static PRTime timestamp = 0;
  if (!timestamp)
    timestamp = PR_Now();

  // Create a new bookmark folder for the root.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks "
      "(type, position, title, dateAdded, lastModified, guid, parent) "
    "VALUES (:item_type, :item_position, :item_title,"
            ":date_added, :last_modified, GENERATE_GUID(),"
            "IFNULL((SELECT id FROM moz_bookmarks WHERE parent = 0), 0))"
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
  rv = stmt->Execute();
  if (NS_FAILED(rv)) return rv;

  // Create an entry in moz_bookmarks_roots to link the folder to the root.
  nsCOMPtr<mozIStorageStatement> newRootStmt;
  rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks_roots (root_name, folder_id) "
    "VALUES (:root_name, "
              "(SELECT id from moz_bookmarks WHERE "
              " position = :item_position AND "
              " parent = IFNULL((SELECT MIN(folder_id) FROM moz_bookmarks_roots), 0)))"
  ), getter_AddRefs(newRootStmt));
  if (NS_FAILED(rv)) return rv;

  rv = newRootStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("root_name"),
                                         aRootName);
  if (NS_FAILED(rv)) return rv;
  rv = newRootStmt->BindInt32ByName(NS_LITERAL_CSTRING("item_position"),
                                    itemPosition);
  if (NS_FAILED(rv)) return rv;
  rv = newRootStmt->Execute();
  if (NS_FAILED(rv)) return rv;

  // The 'places' root is a folder containing the other roots.
  // The first bookmark in a folder has position 0.
  if (!aRootName.Equals("places"))
    ++itemPosition;

  return NS_OK;
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
#ifdef MOZ_ANDROID_HISTORY
  // Currently places has deeply weaved it way throughout the gecko codebase.
  // Here we disable all database creation and loading of places.
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

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
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA page_size"
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
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA temp_store = MEMORY"));
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

      if (currentSchemaVersion < 6) {
        // These are early Firefox 3.0 alpha versions that are not supported
        // anymore.  In this case it's safer to just replace the database.
        return NS_ERROR_FILE_CORRUPTED;
      }

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

      if (currentSchemaVersion < 13) {
        rv = MigrateV13Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 14) {
        rv = MigrateV14Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 15) {
        rv = MigrateV15Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 16) {
        rv = MigrateV16Up();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 11 uses schema version 16.

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

    // moz_hosts.
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_HOSTS);
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

    // Initialize the bookmark roots in the new DB.
    rv = CreateBookmarkRoots();
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
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("places"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  // Fetch the internationalized folder name from the string bundle.
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("BookmarksMenuFolderTitle").get(),
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("menu"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("BookmarksToolbarFolderTitle").get(),
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("toolbar"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("TagsFolderTitle").get(),
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("tags"), rootTitle);
  if (NS_FAILED(rv)) return rv;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("UnsortedBookmarksFolderTitle").get(),
                                 getter_Copies(rootTitle));
  if (NS_FAILED(rv)) return rv;
  rv = CreateRoot(mMainConn, NS_LITERAL_CSTRING("unfiled"), rootTitle);
  if (NS_FAILED(rv)) return rv;

#if DEBUG
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "(SELECT COUNT(*) FROM moz_bookmarks), "
      "(SELECT COUNT(*) FROM moz_bookmarks_roots), "
      "(SELECT SUM(position) FROM moz_bookmarks WHERE "
        "id IN (SELECT folder_id FROM moz_bookmarks_roots))"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(hasResult);
  PRInt32 bookmarkCount = 0;
  rv = stmt->GetInt32(0, &bookmarkCount);
  if (NS_FAILED(rv)) return rv;
  PRInt32 rootCount = 0;
  rv = stmt->GetInt32(1, &rootCount);
  if (NS_FAILED(rv)) return rv;
  PRInt32 positionSum = 0;
  rv = stmt->GetInt32(2, &positionSum);
  if (NS_FAILED(rv)) return rv;
  MOZ_ASSERT(bookmarkCount == 5 && rootCount == 5 && positionSum == 6);
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

  // Add the triggers that update the moz_hosts table as necessary.
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERUPDATE_FRECENCY_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMainConn->ExecuteSimpleSQL(CREATE_PLACES_AFTERUPDATE_TYPED_TRIGGER);
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
    "UPDATE moz_bookmarks SET title = :new_title WHERE id = "
      "(SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = :root_name)"
  ), getter_AddRefs(stmt));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  rv = stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  if (NS_FAILED(rv)) return rv;

  const char *rootNames[] = { "menu", "toolbar", "tags", "unfiled" };
  const char *titleStringIDs[] = {
    "BookmarksMenuFolderTitle", "BookmarksToolbarFolderTitle",
    "TagsFolderTitle", "UnsortedBookmarksFolderTitle"
  };

  for (PRUint32 i = 0; i < ArrayLength(rootNames); ++i) {
    nsXPIDLString title;
    rv = bundle->GetStringFromName(NS_ConvertASCIItoUTF16(titleStringIDs[i]).get(),
                                   getter_Copies(title));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<mozIStorageBindingParams> params;
    rv = paramsArray->NewBindingParams(getter_AddRefs(params));
    if (NS_FAILED(rv)) return rv;
    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("root_name"),
                                      nsDependentCString(rootNames[i]));
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
  rv = stmt->ExecuteAsync(nsnull, getter_AddRefs(pendingStmt));
  if (NS_FAILED(rv)) return rv;

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

  // Some old v6 databases come from alpha versions that missed indices.
  // Just bail out and replace the database in such a case.
  bool URLUniqueIndexExists = false;
  nsresult rv = mMainConn->IndexExists(NS_LITERAL_CSTRING(
    "moz_places_url_uniqueindex"
  ), &URLUniqueIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!URLUniqueIndexExists) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  mozStorageTransaction transaction(mMainConn, false);

  // We need an index on lastModified to catch quickly last modified bookmark
  // title for tag container's children. This will be useful for Sync, too.
  bool lastModIndexExists = false;
  rv = mMainConn->IndexExists(
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
    nsCOMPtr<mozIStorageAsyncStatement> stmt = GetAsyncStatement(
      "UPDATE moz_places SET frecency = ( "
        "CASE "
        "WHEN url BETWEEN 'place:' AND 'place;' "
        "THEN 0 "
        "ELSE -1 "
        "END "
      ") "
    );
    NS_ENSURE_STATE(stmt);
    nsCOMPtr<mozIStoragePendingStatement> ps;
    (void)stmt->ExecuteAsync(nsnull, getter_AddRefs(ps));
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
  NS_ENSURE_SUCCESS(rv, rv);
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
  }

  // Add the moz_inputhistory table, if missing.
  bool tableExists = false;
  rv = mMainConn->TableExists(NS_LITERAL_CSTRING("moz_inputhistory"),
                              &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!tableExists) {
    rv = mMainConn->ExecuteSimpleSQL(CREATE_MOZ_INPUTHISTORY);
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
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_annos_place_idindex"));
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

    // moz_places grew a guid column. Add the column, but do not populate it
    // with anything just yet. We will do that soon.
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
  rv = deleteDynContainersStmt->ExecuteAsync(nsnull, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
Database::MigrateV14Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // For existing profiles, we may not have a moz_favicons.guid column.
  // Add it here. We want it to be unique, but ALTER TABLE doesn't allow
  // a uniqueness constraint, so the index must be created separately.
  nsCOMPtr<mozIStorageStatement> hasGuidStatement;
  nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT guid FROM moz_favicons"),
    getter_AddRefs(hasGuidStatement));

  if (NS_FAILED(rv)) {
    rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_favicons "
      "ADD COLUMN guid TEXT"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMainConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_FAVICONS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Generate GUID for any favicon missing it.
  rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_favicons "
    "SET guid = GENERATE_GUID() "
    "WHERE guid ISNULL "
  ));
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
Database::MigrateV16Up()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Due to Bug 715268 downgraded and then upgraded profiles may lack favicons
  // guids, so fillup any missing ones.
  nsresult rv = mMainConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_favicons "
    "SET guid = GENERATE_GUID() "
    "WHERE guid ISNULL "
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
  rv = fillHostsStmt->ExecuteAsync(nsnull, getter_AddRefs(ps));
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
  rv = updateTypedStmt->ExecuteAsync(nsnull, getter_AddRefs(ps));
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

  // Update prefixes.
  nsCOMPtr<mozIStorageAsyncStatement> updatePrefixesStmt;
  rv = mMainConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_hosts SET prefix = ( "
      HOSTS_PREFIX_PRIORITY_FRAGMENT
    ") "
  ), getter_AddRefs(updatePrefixesStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = updatePrefixesStmt->ExecuteAsync(nsnull, getter_AddRefs(ps));
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

  nsRefPtr<BlockingConnectionCloseCallback> closeListener =
    new BlockingConnectionCloseCallback();
  (void)mMainConn->AsyncClose(closeListener);
  closeListener->Spin();

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
  MOZ_ASSERT(NS_IsMainThread());
 
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
      bool haveNullGuids = false;
      nsCOMPtr<mozIStorageStatement> stmt;

      nsresult rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT 1 "
        "FROM moz_places "
        "WHERE guid IS NULL "
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->ExecuteStep(&haveNullGuids);
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(!haveNullGuids && "Found a page without a GUID!");

      rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT 1 "
        "FROM moz_bookmarks "
        "WHERE guid IS NULL "
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->ExecuteStep(&haveNullGuids);
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(!haveNullGuids && "Found a bookmark without a GUID!");

      rv = mMainConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT 1 "
        "FROM moz_favicons "
        "WHERE guid IS NULL "
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->ExecuteStep(&haveNullGuids);
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(!haveNullGuids && "Found a favicon without a GUID!");
    }
#endif

    // As the last step in the shutdown path, finalize the database handle.
    Shutdown();
  }

  return NS_OK;
}

} // namespace places
} // namespace mozilla
