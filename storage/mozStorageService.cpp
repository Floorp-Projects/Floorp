/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "mozStorageService.h"
#include "mozStorageConnection.h"
#include "nsAutoPtr.h"
#include "nsCollationCID.h"
#include "nsEmbedCID.h"
#include "nsExceptionHandler.h"
#include "nsThreadUtils.h"
#include "mozStoragePrivateHelpers.h"
#include "nsIXPConnect.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/LateWriteChecks.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStoragePendingStatement.h"

#include "sqlite3.h"
#include "mozilla/AutoSQLiteLifetime.h"

#ifdef XP_WIN
// "windows.h" was included and it can #define lots of things we care about...
#undef CompareString
#endif

#include "nsIPromptService.h"

////////////////////////////////////////////////////////////////////////////////
//// Defines

#define PREF_TS_SYNCHRONOUS "toolkit.storage.synchronous"
#define PREF_TS_SYNCHRONOUS_DEFAULT 1

#define PREF_TS_PAGESIZE "toolkit.storage.pageSize"

// This value must be kept in sync with the value of SQLITE_DEFAULT_PAGE_SIZE in
// db/sqlite3/src/Makefile.in.
#define PREF_TS_PAGESIZE_DEFAULT 32768

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Memory Reporting

#ifdef MOZ_DMD
static mozilla::Atomic<size_t> gSqliteMemoryUsed;
#endif

static int64_t
StorageSQLiteDistinguishedAmount()
{
  return ::sqlite3_memory_used();
}

/**
 * Passes a single SQLite memory statistic to a memory reporter callback.
 *
 * @param aHandleReport
 *        The callback.
 * @param aData
 *        The data for the callback.
 * @param aConn
 *        The SQLite connection.
 * @param aPathHead
 *        Head of the path for the memory report.
 * @param aKind
 *        The memory report statistic kind, one of "stmt", "cache" or
 *        "schema".
 * @param aDesc
 *        The memory report description.
 * @param aOption
 *        The SQLite constant for getting the measurement.
 * @param aTotal
 *        The accumulator for the measurement.
 */
static void
ReportConn(nsIHandleReportCallback *aHandleReport,
           nsISupports *aData,
           Connection *aConn,
           const nsACString &aPathHead,
           const nsACString &aKind,
           const nsACString &aDesc,
           int32_t aOption,
           size_t *aTotal)
{
  nsCString path(aPathHead);
  path.Append(aKind);
  path.AppendLiteral("-used");

  int32_t val = aConn->getSqliteRuntimeStatus(aOption);
  aHandleReport->Callback(EmptyCString(), path,
                          nsIMemoryReporter::KIND_HEAP,
                          nsIMemoryReporter::UNITS_BYTES,
                          int64_t(val), aDesc, aData);
  *aTotal += val;
}

// Warning: To get a Connection's measurements requires holding its lock.
// There may be a delay getting the lock if another thread is accessing the
// Connection.  This isn't very nice if CollectReports is called from the main
// thread!  But at the time of writing this function is only called when
// about:memory is loaded (not, for example, when telemetry pings occur) and
// any delays in that case aren't so bad.
NS_IMETHODIMP
Service::CollectReports(nsIHandleReportCallback *aHandleReport,
                        nsISupports *aData, bool aAnonymize)
{
  size_t totalConnSize = 0;
  {
    nsTArray<RefPtr<Connection> > connections;
    getConnections(connections);

    for (uint32_t i = 0; i < connections.Length(); i++) {
      RefPtr<Connection> &conn = connections[i];

      // Someone may have closed the Connection, in which case we skip it.
      // Note that we have consumers of the synchronous API that are off the
      // main-thread, like the DOM Cache and IndexedDB, and as such we must be
      // sure that we have a connection.
      MutexAutoLock lockedAsyncScope(conn->sharedAsyncExecutionMutex);
      if (!conn->connectionReady()) {
          continue;
      }

      nsCString pathHead("explicit/storage/sqlite/");
      // This filename isn't privacy-sensitive, and so is never anonymized.
      pathHead.Append(conn->getFilename());
      pathHead.Append('/');

      SQLiteMutexAutoLock lockedScope(conn->sharedDBMutex);

      NS_NAMED_LITERAL_CSTRING(stmtDesc,
        "Memory (approximate) used by all prepared statements used by "
        "connections to this database.");
      ReportConn(aHandleReport, aData, conn, pathHead,
                 NS_LITERAL_CSTRING("stmt"), stmtDesc,
                 SQLITE_DBSTATUS_STMT_USED, &totalConnSize);

      NS_NAMED_LITERAL_CSTRING(cacheDesc,
        "Memory (approximate) used by all pager caches used by connections "
        "to this database.");
      ReportConn(aHandleReport, aData, conn, pathHead,
                 NS_LITERAL_CSTRING("cache"), cacheDesc,
                 SQLITE_DBSTATUS_CACHE_USED_SHARED, &totalConnSize);

      NS_NAMED_LITERAL_CSTRING(schemaDesc,
        "Memory (approximate) used to store the schema for all databases "
        "associated with connections to this database.");
      ReportConn(aHandleReport, aData, conn, pathHead,
                 NS_LITERAL_CSTRING("schema"), schemaDesc,
                 SQLITE_DBSTATUS_SCHEMA_USED, &totalConnSize);
    }

#ifdef MOZ_DMD
    if (::sqlite3_memory_used() != int64_t(gSqliteMemoryUsed)) {
      NS_WARNING("memory consumption reported by SQLite doesn't match "
                 "our measurements");
    }
#endif
  }

  int64_t other = ::sqlite3_memory_used() - totalConnSize;

  MOZ_COLLECT_REPORT(
    "explicit/storage/sqlite/other", KIND_HEAP, UNITS_BYTES, other,
    "All unclassified sqlite memory.");

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// Service

NS_IMPL_ISUPPORTS(
  Service,
  mozIStorageService,
  nsIObserver,
  nsIMemoryReporter
)

Service *Service::gService = nullptr;

already_AddRefed<Service>
Service::getSingleton()
{
  if (gService) {
    return do_AddRef(gService);
  }

  // Ensure that we are using the same version of SQLite that we compiled with
  // or newer.  Our configure check ensures we are using a new enough version
  // at compile time.
  if (SQLITE_VERSION_NUMBER > ::sqlite3_libversion_number()) {
    nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    if (ps) {
      nsAutoString title, message;
      title.AppendLiteral("SQLite Version Error");
      message.AppendLiteral("The application has been updated, but the SQLite "
                            "library wasn't updated properly and the application "
                            "cannot run. Please try to launch the application again. "
                            "If that should still fail, please try reinstalling "
                            "it, or visit https://support.mozilla.org/.");
      (void)ps->Alert(nullptr, title.get(), message.get());
    }
    MOZ_CRASH("SQLite Version Error");
  }

  // The first reference to the storage service must be obtained on the
  // main thread.
  NS_ENSURE_TRUE(NS_IsMainThread(), nullptr);
  RefPtr<Service> service = new Service();
  if (NS_SUCCEEDED(service->initialize())) {
    // Note: This is cleared in the Service destructor.
    gService = service.get();
    return service.forget();
  }

  return nullptr;
}

int32_t Service::sSynchronousPref;

// static
int32_t
Service::getSynchronousPref()
{
  return sSynchronousPref;
}

int32_t Service::sDefaultPageSize = PREF_TS_PAGESIZE_DEFAULT;

Service::Service()
: mMutex("Service::mMutex")
, mSqliteVFS(nullptr)
, mRegistrationMutex("Service::mRegistrationMutex")
, mConnections()
{
}

Service::~Service()
{
  mozilla::UnregisterWeakMemoryReporter(this);
  mozilla::UnregisterStorageSQLiteDistinguishedAmount();

  int rc = sqlite3_vfs_unregister(mSqliteVFS);
  if (rc != SQLITE_OK)
    NS_WARNING("Failed to unregister sqlite vfs wrapper.");

  gService = nullptr;
  delete mSqliteVFS;
  mSqliteVFS = nullptr;
}

void
Service::registerConnection(Connection *aConnection)
{
  mRegistrationMutex.AssertNotCurrentThreadOwns();
  MutexAutoLock mutex(mRegistrationMutex);
  (void)mConnections.AppendElement(aConnection);
}

void
Service::unregisterConnection(Connection *aConnection)
{
  // If this is the last Connection it might be the only thing keeping Service
  // alive.  So ensure that Service is destroyed only after the Connection is
  // cleanly unregistered and destroyed.
  RefPtr<Service> kungFuDeathGrip(this);
  RefPtr<Connection> forgettingRef;
  {
    mRegistrationMutex.AssertNotCurrentThreadOwns();
    MutexAutoLock mutex(mRegistrationMutex);

    for (uint32_t i = 0 ; i < mConnections.Length(); ++i) {
      if (mConnections[i] == aConnection) {
        // Because dropping the final reference can potentially result in
        // spinning a nested event loop if the connection was not properly
        // shutdown, we want to do that outside this loop so that we can finish
        // mutating the array and drop our mutex.
        forgettingRef = mConnections[i].forget();
        mConnections.RemoveElementAt(i);
        break;
      }
    }
  }

  MOZ_ASSERT(forgettingRef,
             "Attempt to unregister unknown storage connection!");

  // Ensure the connection is released on its opening thread.  We explicitly use
  // aAlwaysDispatch=false because at the time of writing this, LocalStorage's
  // StorageDBThread uses a hand-rolled PRThread implementation that cannot
  // handle us dispatching events at it during shutdown.  However, it is
  // arguably also desirable for callers to not be aware of our connection
  // tracking mechanism.  And by synchronously dropping the reference (when
  // on the correct thread), this avoids surprises for the caller and weird
  // shutdown edge cases.
  nsCOMPtr<nsIThread> thread = forgettingRef->threadOpenedOn;
  NS_ProxyRelease(
    "storage::Service::mConnections", thread, forgettingRef.forget(), false);
}

void
Service::getConnections(/* inout */ nsTArray<RefPtr<Connection> >& aConnections)
{
  mRegistrationMutex.AssertNotCurrentThreadOwns();
  MutexAutoLock mutex(mRegistrationMutex);
  aConnections.Clear();
  aConnections.AppendElements(mConnections);
}

void
Service::minimizeMemory()
{
  nsTArray<RefPtr<Connection> > connections;
  getConnections(connections);

  for (uint32_t i = 0; i < connections.Length(); i++) {
    RefPtr<Connection> conn = connections[i];
    // For non-main-thread owning/opening threads, we may be racing against them
    // closing their connection or their thread.  That's okay, see below.
    if (!conn->connectionReady())
      continue;

    NS_NAMED_LITERAL_CSTRING(shrinkPragma, "PRAGMA shrink_memory");
    nsCOMPtr<mozIStorageConnection> syncConn = do_QueryInterface(
      NS_ISUPPORTS_CAST(mozIStorageAsyncConnection*, conn));
    bool onOpenedThread = false;

    if (!syncConn) {
      // This is a mozIStorageAsyncConnection, it can only be used on the main
      // thread, so we can do a straight API call.
      nsCOMPtr<mozIStoragePendingStatement> ps;
      DebugOnly<nsresult> rv =
        conn->ExecuteSimpleSQLAsync(shrinkPragma, nullptr, getter_AddRefs(ps));
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Should have purged sqlite caches");
    } else if (NS_SUCCEEDED(conn->threadOpenedOn->IsOnCurrentThread(&onOpenedThread)) &&
               onOpenedThread) {
      if (conn->isAsyncExecutionThreadAvailable()) {
        nsCOMPtr<mozIStoragePendingStatement> ps;
        DebugOnly<nsresult> rv =
          conn->ExecuteSimpleSQLAsync(shrinkPragma, nullptr, getter_AddRefs(ps));
        MOZ_ASSERT(NS_SUCCEEDED(rv), "Should have purged sqlite caches");
      } else {
        conn->ExecuteSimpleSQL(shrinkPragma);
      }
    } else {
      // We are on the wrong thread, the query should be executed on the
      // opener thread, so we must dispatch to it.
      // It's possible the connection is already closed or will be closed by the
      // time our runnable runs.  ExecuteSimpleSQL will safely return with a
      // failure in that case.  If the thread is shutting down or shut down, the
      // dispatch will fail and that's okay.
      nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod<const nsCString>(
          "Connection::ExecuteSimpleSQL",
          conn, &Connection::ExecuteSimpleSQL, shrinkPragma);
      Unused << conn->threadOpenedOn->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
}

sqlite3_vfs *ConstructTelemetryVFS();
const char *GetVFSName();

static const char* sObserverTopics[] = {
  "memory-pressure",
  "xpcom-shutdown-threads"
};

nsresult
Service::initialize()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be initialized on the main thread");

  int rc = AutoSQLiteLifetime::getInitResult();
  if (rc != SQLITE_OK)
    return convertResultCode(rc);

  mSqliteVFS = ConstructTelemetryVFS();
  if (mSqliteVFS) {
    rc = sqlite3_vfs_register(mSqliteVFS, 0);
    if (rc != SQLITE_OK)
      return convertResultCode(rc);
  } else {
    NS_WARNING("Failed to register telemetry VFS");
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(os, NS_ERROR_FAILURE);

  for (size_t i = 0; i < ArrayLength(sObserverTopics); ++i) {
    nsresult rv = os->AddObserver(this, sObserverTopics[i], false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // We need to obtain the toolkit.storage.synchronous preferences on the main
  // thread because the preference service can only be accessed there.  This
  // is cached in the service for all future Open[Unshared]Database calls.
  sSynchronousPref =
    Preferences::GetInt(PREF_TS_SYNCHRONOUS, PREF_TS_SYNCHRONOUS_DEFAULT);

  // We need to obtain the toolkit.storage.pageSize preferences on the main
  // thread because the preference service can only be accessed there.  This
  // is cached in the service for all future Open[Unshared]Database calls.
  sDefaultPageSize =
      Preferences::GetInt(PREF_TS_PAGESIZE, PREF_TS_PAGESIZE_DEFAULT);

  mozilla::RegisterWeakMemoryReporter(this);
  mozilla::RegisterStorageSQLiteDistinguishedAmount(StorageSQLiteDistinguishedAmount);

  return NS_OK;
}

int
Service::localeCompareStrings(const nsAString &aStr1,
                              const nsAString &aStr2,
                              int32_t aComparisonStrength)
{
  // The implementation of nsICollation.CompareString() is platform-dependent.
  // On Linux it's not thread-safe.  It may not be on Windows and OS X either,
  // but it's more difficult to tell.  We therefore synchronize this method.
  MutexAutoLock mutex(mMutex);

  nsICollation *coll = getLocaleCollation();
  if (!coll) {
    NS_ERROR("Storage service has no collation");
    return 0;
  }

  int32_t res;
  nsresult rv = coll->CompareString(aComparisonStrength, aStr1, aStr2, &res);
  if (NS_FAILED(rv)) {
    NS_ERROR("Collation compare string failed");
    return 0;
  }

  return res;
}

nsICollation *
Service::getLocaleCollation()
{
  mMutex.AssertCurrentThreadOwns();

  if (mLocaleCollation)
    return mLocaleCollation;

  nsCOMPtr<nsICollationFactory> collFact =
    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  if (!collFact) {
    NS_WARNING("Could not create collation factory");
    return nullptr;
  }

  nsresult rv = collFact->CreateCollation(getter_AddRefs(mLocaleCollation));
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not create collation");
    return nullptr;
  }

  return mLocaleCollation;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageService


NS_IMETHODIMP
Service::OpenSpecialDatabase(const char *aStorageKey,
                             mozIStorageConnection **_connection)
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageFile;
  if (::strcmp(aStorageKey, "memory") == 0) {
    // just fall through with nullptr storageFile, this will cause the storage
    // connection to use a memory DB.
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Connection> msc = new Connection(this, SQLITE_OPEN_READWRITE, false);

  rv = storageFile ? msc->initialize(storageFile) : msc->initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  msc.forget(_connection);
  return NS_OK;

}

namespace {

class AsyncInitDatabase final : public Runnable
{
public:
  AsyncInitDatabase(Connection* aConnection,
                    nsIFile* aStorageFile,
                    int32_t aGrowthIncrement,
                    mozIStorageCompletionCallback* aCallback)
    : Runnable("storage::AsyncInitDatabase")
    , mConnection(aConnection)
    , mStorageFile(aStorageFile)
    , mGrowthIncrement(aGrowthIncrement)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    nsresult rv = mConnection->initializeOnAsyncThread(mStorageFile);
    if (NS_FAILED(rv)) {
      return DispatchResult(rv, nullptr);
    }

    if (mGrowthIncrement >= 0) {
      // Ignore errors. In the future, we might wish to log them.
      (void)mConnection->SetGrowthIncrement(mGrowthIncrement, EmptyCString());
    }

    return DispatchResult(NS_OK, NS_ISUPPORTS_CAST(mozIStorageAsyncConnection*,
                          mConnection));
  }

private:
  nsresult DispatchResult(nsresult aStatus, nsISupports* aValue) {
    RefPtr<CallbackComplete> event =
      new CallbackComplete(aStatus,
                           aValue,
                           mCallback.forget());
    return NS_DispatchToMainThread(event);
  }

  ~AsyncInitDatabase()
  {
    NS_ReleaseOnMainThreadSystemGroup(
      "AsyncInitDatabase::mStorageFile", mStorageFile.forget());
    NS_ReleaseOnMainThreadSystemGroup(
      "AsyncInitDatabase::mConnection", mConnection.forget());

    // Generally, the callback will be released by CallbackComplete.
    // However, if for some reason Run() is not executed, we still
    // need to ensure that it is released here.
    NS_ReleaseOnMainThreadSystemGroup(
      "AsyncInitDatabase::mCallback", mCallback.forget());
  }

  RefPtr<Connection> mConnection;
  nsCOMPtr<nsIFile> mStorageFile;
  int32_t mGrowthIncrement;
  RefPtr<mozIStorageCompletionCallback> mCallback;
};

} // namespace

NS_IMETHODIMP
Service::OpenAsyncDatabase(nsIVariant *aDatabaseStore,
                           nsIPropertyBag2 *aOptions,
                           mozIStorageCompletionCallback *aCallback)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }
  NS_ENSURE_ARG(aDatabaseStore);
  NS_ENSURE_ARG(aCallback);

  nsresult rv;
  bool shared = false;
  bool readOnly = false;
  bool ignoreLockingMode = false;
  int32_t growthIncrement = -1;

#define FAIL_IF_SET_BUT_INVALID(rv)\
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE) { \
    return NS_ERROR_INVALID_ARG; \
  }

  // Deal with options first:
  if (aOptions) {
    rv = aOptions->GetPropertyAsBool(NS_LITERAL_STRING("readOnly"), &readOnly);
    FAIL_IF_SET_BUT_INVALID(rv);

    rv = aOptions->GetPropertyAsBool(NS_LITERAL_STRING("ignoreLockingMode"),
                                     &ignoreLockingMode);
    FAIL_IF_SET_BUT_INVALID(rv);
    // Specifying ignoreLockingMode will force use of the readOnly flag:
    if (ignoreLockingMode) {
      readOnly = true;
    }

    rv = aOptions->GetPropertyAsBool(NS_LITERAL_STRING("shared"), &shared);
    FAIL_IF_SET_BUT_INVALID(rv);

    // NB: we re-set to -1 if we don't have a storage file later on.
    rv = aOptions->GetPropertyAsInt32(NS_LITERAL_STRING("growthIncrement"),
                                      &growthIncrement);
    FAIL_IF_SET_BUT_INVALID(rv);
  }
  int flags = readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;

  nsCOMPtr<nsIFile> storageFile;
  nsCOMPtr<nsISupports> dbStore;
  rv = aDatabaseStore->GetAsISupports(getter_AddRefs(dbStore));
  if (NS_SUCCEEDED(rv)) {
    // Generally, aDatabaseStore holds the database nsIFile.
    storageFile = do_QueryInterface(dbStore, &rv);
    if (NS_FAILED(rv)) {
      return NS_ERROR_INVALID_ARG;
    }

    rv = storageFile->Clone(getter_AddRefs(storageFile));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (!readOnly) {
      // Ensure that SQLITE_OPEN_CREATE is passed in for compatibility reasons.
      flags |= SQLITE_OPEN_CREATE;
    }

    // Apply the shared-cache option.
    flags |= shared ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE;
  } else {
    // Sometimes, however, it's a special database name.
    nsAutoCString keyString;
    rv = aDatabaseStore->GetAsACString(keyString);
    if (NS_FAILED(rv) || !keyString.EqualsLiteral("memory")) {
      return NS_ERROR_INVALID_ARG;
    }

    // Just fall through with nullptr storageFile, this will cause the storage
    // connection to use a memory DB.
  }

  if (!storageFile && growthIncrement >= 0) {
    return NS_ERROR_INVALID_ARG;
  }

  // Create connection on this thread, but initialize it on its helper thread.
  RefPtr<Connection> msc = new Connection(this, flags, true,
                                          ignoreLockingMode);
  nsCOMPtr<nsIEventTarget> target = msc->getAsyncExecutionTarget();
  MOZ_ASSERT(target, "Cannot initialize a connection that has been closed already");

  RefPtr<AsyncInitDatabase> asyncInit =
    new AsyncInitDatabase(msc,
                          storageFile,
                          growthIncrement,
                          aCallback);
  return target->Dispatch(asyncInit, nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP
Service::OpenDatabase(nsIFile *aDatabaseFile,
                      mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aDatabaseFile);

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE |
              SQLITE_OPEN_CREATE;
  RefPtr<Connection> msc = new Connection(this, flags, false);

  nsresult rv = msc->initialize(aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, rv);

  msc.forget(_connection);
  return NS_OK;
}

NS_IMETHODIMP
Service::OpenUnsharedDatabase(nsIFile *aDatabaseFile,
                              mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aDatabaseFile);

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_PRIVATECACHE |
              SQLITE_OPEN_CREATE;
  RefPtr<Connection> msc = new Connection(this, flags, false);

  nsresult rv = msc->initialize(aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, rv);

  msc.forget(_connection);
  return NS_OK;
}

NS_IMETHODIMP
Service::OpenDatabaseWithFileURL(nsIFileURL *aFileURL,
                                 mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aFileURL);

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE |
              SQLITE_OPEN_CREATE | SQLITE_OPEN_URI;
  RefPtr<Connection> msc = new Connection(this, flags, false);

  nsresult rv = msc->initialize(aFileURL);
  NS_ENSURE_SUCCESS(rv, rv);

  msc.forget(_connection);
  return NS_OK;
}

NS_IMETHODIMP
Service::BackupDatabaseFile(nsIFile *aDBFile,
                            const nsAString &aBackupFileName,
                            nsIFile *aBackupParentDirectory,
                            nsIFile **backup)
{
  nsresult rv;
  nsCOMPtr<nsIFile> parentDir = aBackupParentDirectory;
  if (!parentDir) {
    // This argument is optional, and defaults to the same parent directory
    // as the current file.
    rv = aDBFile->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIFile> backupDB;
  rv = parentDir->Clone(getter_AddRefs(backupDB));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = backupDB->Append(aBackupFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = backupDB->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString fileName;
  rv = backupDB->GetLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = backupDB->Remove(false);
  NS_ENSURE_SUCCESS(rv, rv);

  backupDB.forget(backup);

  return aDBFile->CopyTo(parentDir, fileName);
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
Service::Observe(nsISupports *, const char *aTopic, const char16_t *)
{
  if (strcmp(aTopic, "memory-pressure") == 0) {
    minimizeMemory();
  } else if (strcmp(aTopic, "xpcom-shutdown-threads") == 0) {
    // The Service is kept alive by our strong observer references and
    // references held by Connection instances.  Since we're about to remove the
    // former and then wait for the latter ones to go away, it behooves us to
    // hold a strong reference to ourselves so our calls to getConnections() do
    // not happen on a deleted object.
    RefPtr<Service> kungFuDeathGrip = this;

    nsCOMPtr<nsIObserverService> os =
      mozilla::services::GetObserverService();

    for (size_t i = 0; i < ArrayLength(sObserverTopics); ++i) {
      (void)os->RemoveObserver(this, sObserverTopics[i]);
    }

    SpinEventLoopUntil([&]() -> bool {
      // We must wait until all the closing connections are closed.
      nsTArray<RefPtr<Connection>> connections;
      getConnections(connections);
      for (auto& conn : connections) {
        if (conn->isClosing()) {
          return false;
        }
      }
      return true;
    });

    if (gShutdownChecks == SCM_CRASH) {
      nsTArray<RefPtr<Connection> > connections;
      getConnections(connections);
      for (uint32_t i = 0, n = connections.Length(); i < n; i++) {
        if (!connections[i]->isClosed()) {
          // getFilename is only the leaf name for the database file,
          // so it shouldn't contain privacy-sensitive information.
          CrashReporter::AnnotateCrashReport(
            NS_LITERAL_CSTRING("StorageConnectionNotClosed"),
            connections[i]->getFilename());
#ifdef DEBUG
          printf_stderr("Storage connection not closed: %s",
                        connections[i]->getFilename().get());
#endif
          MOZ_CRASH();
        }
      }
    }
  }

  return NS_OK;
}

} // namespace storage
} // namespace mozilla
