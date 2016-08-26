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
#include "nsThreadUtils.h"
#include "mozStoragePrivateHelpers.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIXPConnect.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/LateWriteChecks.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStoragePendingStatement.h"

#include "sqlite3.h"

#ifdef SQLITE_OS_WIN
// "windows.h" was included and it can #define lots of things we care about...
#undef CompareString
#endif

#include "nsIPromptService.h"

#ifdef MOZ_STORAGE_MEMORY
#  include "mozmemory.h"
#  ifdef MOZ_DMD
#    include "DMD.h"
#  endif
#endif

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
      bool isReady;
      (void)conn->GetConnectionReady(&isReady);
      if (!isReady) {
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
                 SQLITE_DBSTATUS_CACHE_USED, &totalConnSize);

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

Service *
Service::getSingleton()
{
  if (gService) {
    NS_ADDREF(gService);
    return gService;
  }

  // Ensure that we are using the same version of SQLite that we compiled with
  // or newer.  Our configure check ensures we are using a new enough version
  // at compile time.
  if (SQLITE_VERSION_NUMBER > ::sqlite3_libversion_number()) {
    nsCOMPtr<nsIPromptService> ps(do_GetService(NS_PROMPTSERVICE_CONTRACTID));
    if (ps) {
      nsAutoString title, message;
      title.AppendLiteral("SQLite Version Error");
      message.AppendLiteral("The application has been updated, but your version "
                          "of SQLite is too old and the application cannot "
                          "run.");
      (void)ps->Alert(nullptr, title.get(), message.get());
    }
    MOZ_CRASH("SQLite Version Error");
  }

  // The first reference to the storage service must be obtained on the
  // main thread.
  NS_ENSURE_TRUE(NS_IsMainThread(), nullptr);
  gService = new Service();
  if (gService) {
    NS_ADDREF(gService);
    if (NS_FAILED(gService->initialize()))
      NS_RELEASE(gService);
  }

  return gService;
}

nsIXPConnect *Service::sXPConnect = nullptr;

// static
already_AddRefed<nsIXPConnect>
Service::getXPConnect()
{
  NS_PRECONDITION(NS_IsMainThread(),
                  "Must only get XPConnect on the main thread!");
  NS_PRECONDITION(gService,
                  "Can not get XPConnect without an instance of our service!");

  // If we've been shutdown, sXPConnect will be null.  To prevent leaks, we do
  // not cache the service after this point.
  nsCOMPtr<nsIXPConnect> xpc(sXPConnect);
  if (!xpc)
    xpc = do_GetService(nsIXPConnect::GetCID());
  NS_ASSERTION(xpc, "Could not get XPConnect!");
  return xpc.forget();
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

  // Shutdown the sqlite3 API.  Warn if shutdown did not turn out okay, but
  // there is nothing actionable we can do in that case.
  rc = ::sqlite3_shutdown();
  if (rc != SQLITE_OK)
    NS_WARNING("sqlite3 did not shutdown cleanly.");

  DebugOnly<bool> shutdownObserved = !sXPConnect;
  NS_ASSERTION(shutdownObserved, "Shutdown was not observed!");

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
  {
    mRegistrationMutex.AssertNotCurrentThreadOwns();
    MutexAutoLock mutex(mRegistrationMutex);

    for (uint32_t i = 0 ; i < mConnections.Length(); ++i) {
      if (mConnections[i] == aConnection) {
        nsCOMPtr<nsIThread> thread = mConnections[i]->threadOpenedOn;

        // Ensure the connection is released on its opening thread.  Note, we
        // must use .forget().take() so that we can manually cast to an
        // unambiguous nsISupports type.
        NS_ProxyRelease(thread, mConnections[i].forget());

        mConnections.RemoveElementAt(i);
        return;
      }
    }

    MOZ_ASSERT_UNREACHABLE("Attempt to unregister unknown storage connection!");
  }
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
      // We are on the opener thread, so we can just proceed.
      conn->ExecuteSimpleSQL(shrinkPragma);
    } else {
      // We are on the wrong thread, the query should be executed on the
      // opener thread, so we must dispatch to it.
      nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod<const nsCString>(
          conn, &Connection::ExecuteSimpleSQL, shrinkPragma);
      conn->threadOpenedOn->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
}

void
Service::shutdown()
{
  NS_IF_RELEASE(sXPConnect);
}

sqlite3_vfs *ConstructTelemetryVFS();

#ifdef MOZ_STORAGE_MEMORY

namespace {

// By default, SQLite tracks the size of all its heap blocks by adding an extra
// 8 bytes at the start of the block to hold the size.  Unfortunately, this
// causes a lot of 2^N-sized allocations to be rounded up by jemalloc
// allocator, wasting memory.  For example, a request for 1024 bytes has 8
// bytes added, becoming a request for 1032 bytes, and jemalloc rounds this up
// to 2048 bytes, wasting 1012 bytes.  (See bug 676189 for more details.)
//
// So we register jemalloc as the malloc implementation, which avoids this
// 8-byte overhead, and thus a lot of waste.  This requires us to provide a
// function, sqliteMemRoundup(), which computes the actual size that will be
// allocated for a given request.  SQLite uses this function before all
// allocations, and may be able to use any excess bytes caused by the rounding.
//
// Note: the wrappers for malloc, realloc and moz_malloc_usable_size are
// necessary because the sqlite_mem_methods type signatures differ slightly
// from the standard ones -- they use int instead of size_t.  But we don't need
// a wrapper for free.

#ifdef MOZ_DMD

// sqlite does its own memory accounting, and we use its numbers in our memory
// reporters.  But we don't want sqlite's heap blocks to show up in DMD's
// output as unreported, so we mark them as reported when they're allocated and
// mark them as unreported when they are freed.
//
// In other words, we are marking all sqlite heap blocks as reported even
// though we're not reporting them ourselves.  Instead we're trusting that
// sqlite is fully and correctly accounting for all of its heap blocks via its
// own memory accounting.  Well, we don't have to trust it entirely, because
// it's easy to keep track (while doing this DMD-specific marking) of exactly
// how much memory SQLite is using.  And we can compare that against what
// SQLite reports it is using.

MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(SqliteMallocSizeOfOnAlloc)
MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE(SqliteMallocSizeOfOnFree)

#endif

static void *sqliteMemMalloc(int n)
{
  void* p = ::malloc(n);
#ifdef MOZ_DMD
  gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(p);
#endif
  return p;
}

static void sqliteMemFree(void *p)
{
#ifdef MOZ_DMD
  gSqliteMemoryUsed -= SqliteMallocSizeOfOnFree(p);
#endif
  ::free(p);
}

static void *sqliteMemRealloc(void *p, int n)
{
#ifdef MOZ_DMD
  gSqliteMemoryUsed -= SqliteMallocSizeOfOnFree(p);
  void *pnew = ::realloc(p, n);
  if (pnew) {
    gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(pnew);
  } else {
    // realloc failed;  undo the SqliteMallocSizeOfOnFree from above
    gSqliteMemoryUsed += SqliteMallocSizeOfOnAlloc(p);
  }
  return pnew;
#else
  return ::realloc(p, n);
#endif
}

static int sqliteMemSize(void *p)
{
  return ::moz_malloc_usable_size(p);
}

static int sqliteMemRoundup(int n)
{
  n = malloc_good_size(n);

  // jemalloc can return blocks of size 2 and 4, but SQLite requires that all
  // allocations be 8-aligned.  So we round up sub-8 requests to 8.  This
  // wastes a small amount of memory but is obviously safe.
  return n <= 8 ? 8 : n;
}

static int sqliteMemInit(void *p)
{
  return 0;
}

static void sqliteMemShutdown(void *p)
{
}

const sqlite3_mem_methods memMethods = {
  &sqliteMemMalloc,
  &sqliteMemFree,
  &sqliteMemRealloc,
  &sqliteMemSize,
  &sqliteMemRoundup,
  &sqliteMemInit,
  &sqliteMemShutdown,
  nullptr
};

} // namespace

#endif  // MOZ_STORAGE_MEMORY

static const char* sObserverTopics[] = {
  "memory-pressure",
  "xpcom-shutdown",
  "xpcom-shutdown-threads"
};

nsresult
Service::initialize()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be initialized on the main thread");

  int rc;

#ifdef MOZ_STORAGE_MEMORY
  rc = ::sqlite3_config(SQLITE_CONFIG_MALLOC, &memMethods);
  if (rc != SQLITE_OK)
    return convertResultCode(rc);
#endif

  // TODO (bug 1191405): do not preallocate the connections caches until we
  // have figured the impact on our consumers and memory.
  sqlite3_config(SQLITE_CONFIG_PAGECACHE, NULL, 0, 0);

  // Explicitly initialize sqlite3.  Although this is implicitly called by
  // various sqlite3 functions (and the sqlite3_open calls in our case),
  // the documentation suggests calling this directly.  So we do.
  rc = ::sqlite3_initialize();
  if (rc != SQLITE_OK)
    return convertResultCode(rc);

  mSqliteVFS = ConstructTelemetryVFS();
  if (mSqliteVFS) {
    rc = sqlite3_vfs_register(mSqliteVFS, 1);
    if (rc != SQLITE_OK)
      return convertResultCode(rc);
  } else {
    NS_WARNING("Failed to register telemetry VFS");
  }

  // Register for xpcom-shutdown so we can cleanup after ourselves.  The
  // observer service can only be used on the main thread.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(os, NS_ERROR_FAILURE);

  for (size_t i = 0; i < ArrayLength(sObserverTopics); ++i) {
    nsresult rv = os->AddObserver(this, sObserverTopics[i], false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // We cache XPConnect for our language helpers.  XPConnect can only be
  // used on the main thread.
  (void)CallGetService(nsIXPConnect::GetCID(), &sXPConnect);

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

  nsCOMPtr<nsILocaleService> svc(do_GetService(NS_LOCALESERVICE_CONTRACTID));
  if (!svc) {
    NS_WARNING("Could not get locale service");
    return nullptr;
  }

  nsCOMPtr<nsILocale> appLocale;
  nsresult rv = svc->GetApplicationLocale(getter_AddRefs(appLocale));
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not get application locale");
    return nullptr;
  }

  nsCOMPtr<nsICollationFactory> collFact =
    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  if (!collFact) {
    NS_WARNING("Could not create collation factory");
    return nullptr;
  }

  rv = collFact->CreateCollation(appLocale, getter_AddRefs(mLocaleCollation));
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
    : mConnection(aConnection)
    , mStorageFile(aStorageFile)
    , mGrowthIncrement(aGrowthIncrement)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());
    nsresult rv = mStorageFile ? mConnection->initialize(mStorageFile)
                               : mConnection->initialize();
    if (NS_FAILED(rv)) {
      nsCOMPtr<nsIRunnable> closeRunnable =
        NewRunnableMethod<mozIStorageCompletionCallback*>(
          mConnection.get(),
          &Connection::AsyncClose,
          nullptr);
      MOZ_ASSERT(closeRunnable);
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(closeRunnable));

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
    NS_ReleaseOnMainThread(mStorageFile.forget());
    NS_ReleaseOnMainThread(mConnection.forget());

    // Generally, the callback will be released by CallbackComplete.
    // However, if for some reason Run() is not executed, we still
    // need to ensure that it is released here.
    NS_ReleaseOnMainThread(mCallback.forget());
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

  nsCOMPtr<nsIFile> storageFile;
  int flags = SQLITE_OPEN_READWRITE;

  nsCOMPtr<nsISupports> dbStore;
  nsresult rv = aDatabaseStore->GetAsISupports(getter_AddRefs(dbStore));
  if (NS_SUCCEEDED(rv)) {
    // Generally, aDatabaseStore holds the database nsIFile.
    storageFile = do_QueryInterface(dbStore, &rv);
    if (NS_FAILED(rv)) {
      return NS_ERROR_INVALID_ARG;
    }

    rv = storageFile->Clone(getter_AddRefs(storageFile));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Ensure that SQLITE_OPEN_CREATE is passed in for compatibility reasons.
    flags |= SQLITE_OPEN_CREATE;

    // Extract and apply the shared-cache option.
    bool shared = false;
    if (aOptions) {
      rv = aOptions->GetPropertyAsBool(NS_LITERAL_STRING("shared"), &shared);
      if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE) {
        return NS_ERROR_INVALID_ARG;
      }
    }
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

  int32_t growthIncrement = -1;
  if (aOptions && storageFile) {
    rv = aOptions->GetPropertyAsInt32(NS_LITERAL_STRING("growthIncrement"),
                                      &growthIncrement);
    if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Create connection on this thread, but initialize it on its helper thread.
  RefPtr<Connection> msc = new Connection(this, flags, true);
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
  } else if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    shutdown();
  } else if (strcmp(aTopic, "xpcom-shutdown-threads") == 0) {
    nsCOMPtr<nsIObserverService> os =
      mozilla::services::GetObserverService();

    for (size_t i = 0; i < ArrayLength(sObserverTopics); ++i) {
      (void)os->RemoveObserver(this, sObserverTopics[i]);
    }

    bool anyOpen = false;
    do {
      nsTArray<RefPtr<Connection> > connections;
      getConnections(connections);
      anyOpen = false;
      for (uint32_t i = 0; i < connections.Length(); i++) {
        RefPtr<Connection> &conn = connections[i];
        if (conn->isClosing()) {
          anyOpen = true;
          break;
        }
      }
      if (anyOpen) {
        nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
        NS_ProcessNextEvent(thread);
      }
    } while (anyOpen);

    if (gShutdownChecks == SCM_CRASH) {
      nsTArray<RefPtr<Connection> > connections;
      getConnections(connections);
      for (uint32_t i = 0, n = connections.Length(); i < n; i++) {
        if (!connections[i]->isClosed()) {
          MOZ_CRASH();
        }
      }
    }
  }

  return NS_OK;
}

} // namespace storage
} // namespace mozilla
