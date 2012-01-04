/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Brett Wilson <brettw@gmail.com>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *   Drew Willcoxon <adw@mozilla.com>
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

#include "mozStorageService.h"
#include "mozStorageConnection.h"
#include "prinit.h"
#include "pratom.h"
#include "nsAutoPtr.h"
#include "nsCollationCID.h"
#include "nsEmbedCID.h"
#include "nsThreadUtils.h"
#include "mozStoragePrivateHelpers.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsIXPConnect.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

#include "sqlite3.h"
#include "test_quota.h"
#include "test_quota.c"

#ifdef SQLITE_OS_WIN
// "windows.h" was included and it can #define lots of things we care about...
#undef CompareString
#endif

#include "nsIPromptService.h"
#include "nsIMemoryReporter.h"

#include "mozilla/FunctionTimer.h"
#include "mozilla/Util.h"

namespace {

class QuotaCallbackData
{
public:
  QuotaCallbackData(mozIStorageQuotaCallback *aCallback,
                    nsISupports *aUserData)
  : callback(aCallback), userData(aUserData)
  {
    MOZ_COUNT_CTOR(QuotaCallbackData);
  }

  ~QuotaCallbackData()
  {
    MOZ_COUNT_DTOR(QuotaCallbackData);
  }

  static void Callback(const char *zFilename,
                       sqlite3_int64 *piLimit,
                       sqlite3_int64 iSize,
                       void *pArg)
  {
    NS_ASSERTION(zFilename && strlen(zFilename), "Null or empty filename!");
    NS_ASSERTION(piLimit, "Null pointer!");

    QuotaCallbackData *data = static_cast<QuotaCallbackData*>(pArg);
    if (!data) {
      // No callback specified, return immediately.
      return;
    }

    NS_ASSERTION(data->callback, "Should never have a null callback!");

    nsDependentCString filename(zFilename);

    PRInt64 newLimit;
    if (NS_SUCCEEDED(data->callback->QuotaExceeded(filename, *piLimit,
                                                   iSize, data->userData,
                                                   &newLimit))) {
      *piLimit = newLimit;
    }
  }

  static void Destroy(void *aUserData)
  {
    delete static_cast<QuotaCallbackData*>(aUserData);
  }

private:
  nsCOMPtr<mozIStorageQuotaCallback> callback;
  nsCOMPtr<nsISupports> userData;
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// Defines

#define PREF_TS_SYNCHRONOUS "toolkit.storage.synchronous"
#define PREF_TS_SYNCHRONOUS_DEFAULT 1

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Memory Reporting

static PRInt64
GetStorageSQLiteMemoryUsed()
{
  return ::sqlite3_memory_used();
}

// We don't need an "explicit" reporter for total SQLite memory usage, because
// the multi-reporter provides reports that add up to the total.  But it's
// useful to have the total in the "Other Measurements" list in about:memory,
// and more importantly, we also gather the total via telemetry.
NS_MEMORY_REPORTER_IMPLEMENT(StorageSQLite,
    "storage-sqlite",
    KIND_OTHER,
    UNITS_BYTES,
    GetStorageSQLiteMemoryUsed,
    "Memory used by SQLite.")

class StorageSQLiteMultiReporter : public nsIMemoryMultiReporter
{
private:
  Service *mService;    // a weakref because Service contains a strongref to this
  nsCString mStmtDesc;
  nsCString mCacheDesc;
  nsCString mSchemaDesc;

public:
  NS_DECL_ISUPPORTS

  StorageSQLiteMultiReporter(Service *aService) 
  : mService(aService)
  {
    NS_NAMED_LITERAL_CSTRING(mStmtDesc,
      "Memory (approximate) used by all prepared statements used by "
      "connections to this database.");

    NS_NAMED_LITERAL_CSTRING(mCacheDesc,
      "Memory (approximate) used by all pager caches used by connections "
      "to this database.");

    NS_NAMED_LITERAL_CSTRING(mSchemaDesc,
      "Memory (approximate) used to store the schema for all databases "
      "associated with connections to this database.");
  }

  // Warning: To get a Connection's measurements requires holding its lock.
  // There may be a delay getting the lock if another thread is accessing the
  // Connection.  This isn't very nice if CollectReports is called from the
  // main thread!  But at the time of writing this function is only called when
  // about:memory is loaded (not, for example, when telemetry pings occur) and
  // any delays in that case aren't so bad.
  NS_IMETHOD CollectReports(nsIMemoryMultiReporterCallback *aCallback,
                            nsISupports *aClosure)
  {
    size_t totalConnSize = 0;
    {
      nsTArray<nsRefPtr<Connection> > connections;
      mService->getConnections(connections);

      for (PRUint32 i = 0; i < connections.Length(); i++) {
        nsRefPtr<Connection> &conn = connections[i];

        // Someone may have closed the Connection, in which case we skip it.
        bool isReady;
        (void)conn->GetConnectionReady(&isReady);
        if (!isReady) {
            continue;
        }

        nsCString pathHead("explicit/storage/sqlite/");
        pathHead.Append(conn->getFilename());
        pathHead.AppendLiteral("/");

        SQLiteMutexAutoLock lockedScope(conn->sharedDBMutex);

        totalConnSize +=
          doConnMeasurement(aCallback, aClosure, *conn.get(), pathHead,
                            NS_LITERAL_CSTRING("stmt"), mStmtDesc,
                            SQLITE_DBSTATUS_STMT_USED);
        totalConnSize +=
          doConnMeasurement(aCallback, aClosure, *conn.get(), pathHead,
                            NS_LITERAL_CSTRING("cache"), mCacheDesc,
                            SQLITE_DBSTATUS_CACHE_USED);
        totalConnSize +=
          doConnMeasurement(aCallback, aClosure, *conn.get(), pathHead,
                            NS_LITERAL_CSTRING("schema"), mSchemaDesc,
                            SQLITE_DBSTATUS_SCHEMA_USED);
      }
    }

    PRInt64 other = ::sqlite3_memory_used() - totalConnSize;

    aCallback->Callback(NS_LITERAL_CSTRING(""),
                        NS_LITERAL_CSTRING("explicit/storage/sqlite/other"),
                        nsIMemoryReporter::KIND_HEAP,
                        nsIMemoryReporter::UNITS_BYTES, other,
                        NS_LITERAL_CSTRING("All unclassified sqlite memory."),
                        aClosure);

    return NS_OK;
  }

  NS_IMETHOD GetExplicitNonHeap(PRInt64 *aAmount)
  {
    // This reporter doesn't do any non-heap measurements.
    *aAmount = 0;
    return NS_OK;
  }

private:
  /**
   * Passes a single SQLite memory statistic to a memory multi-reporter
   * callback.
   *
   * @param aCallback
   *        The callback.
   * @param aClosure
   *        The closure for the callback.
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
   */
  size_t doConnMeasurement(nsIMemoryMultiReporterCallback *aCallback,
                           nsISupports *aClosure,
                           sqlite3 *aConn,
                           const nsACString &aPathHead,
                           const nsACString &aKind,
                           const nsACString &aDesc,
                           int aOption)
  {
    nsCString path(aPathHead);
    path.Append(aKind);
    path.AppendLiteral("-used");

    int curr = 0, max = 0;
    int rc = ::sqlite3_db_status(aConn, aOption, &curr, &max, 0);
    nsresult rv = convertResultCode(rc);
    NS_ENSURE_SUCCESS(rv, rv);

    aCallback->Callback(NS_LITERAL_CSTRING(""), path,
                        nsIMemoryReporter::KIND_HEAP,
                        nsIMemoryReporter::UNITS_BYTES, PRInt64(curr),
                        aDesc, aClosure);
    return curr;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(
  StorageSQLiteMultiReporter,
  nsIMemoryMultiReporter
)

////////////////////////////////////////////////////////////////////////////////
//// Helpers

class ServiceMainThreadInitializer : public nsRunnable
{
public:
  ServiceMainThreadInitializer(Service *aService,
                               nsIObserver *aObserver,
                               nsIXPConnect **aXPConnectPtr,
                               PRInt32 *aSynchronousPrefValPtr)
  : mService(aService)
  , mObserver(aObserver)
  , mXPConnectPtr(aXPConnectPtr)
  , mSynchronousPrefValPtr(aSynchronousPrefValPtr)
  {
  }

  NS_IMETHOD Run()
  {
    NS_PRECONDITION(NS_IsMainThread(), "Must be running on the main thread!");

    // NOTE:  All code that can only run on the main thread and needs to be run
    //        during initialization should be placed here.  During the off-
    //        chance that storage is initialized on a background thread, this
    //        will ensure everything that isn't threadsafe is initialized in
    //        the right place.

    // Register for xpcom-shutdown so we can cleanup after ourselves.  The
    // observer service can only be used on the main thread.
    nsCOMPtr<nsIObserverService> os =
      mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(os, NS_ERROR_FAILURE);
    nsresult rv = os->AddObserver(mObserver, "xpcom-shutdown", false);
    NS_ENSURE_SUCCESS(rv, rv);

    // We cache XPConnect for our language helpers.  XPConnect can only be
    // used on the main thread.
    (void)CallGetService(nsIXPConnect::GetCID(), mXPConnectPtr);

    // We need to obtain the toolkit.storage.synchronous preferences on the main
    // thread because the preference service can only be accessed there.  This
    // is cached in the service for all future Open[Unshared]Database calls.
    PRInt32 synchronous =
      Preferences::GetInt(PREF_TS_SYNCHRONOUS, PREF_TS_SYNCHRONOUS_DEFAULT);
    ::PR_ATOMIC_SET(mSynchronousPrefValPtr, synchronous);

    // Create and register our SQLite memory reporters.  Registration can only
    // happen on the main thread (otherwise you'll get cryptic crashes).
    mService->mStorageSQLiteReporter = new NS_MEMORY_REPORTER_NAME(StorageSQLite);
    mService->mStorageSQLiteMultiReporter = new StorageSQLiteMultiReporter(mService);
    (void)::NS_RegisterMemoryReporter(mService->mStorageSQLiteReporter);
    (void)::NS_RegisterMemoryMultiReporter(mService->mStorageSQLiteMultiReporter);

    return NS_OK;
  }

private:
  Service *mService;
  nsIObserver *mObserver;
  nsIXPConnect **mXPConnectPtr;
  PRInt32 *mSynchronousPrefValPtr;
};

////////////////////////////////////////////////////////////////////////////////
//// Service

NS_IMPL_THREADSAFE_ISUPPORTS3(
  Service,
  mozIStorageService,
  nsIObserver,
  mozIStorageServiceQuotaManagement
)

Service *Service::gService = nsnull;

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
      title.AppendASCII("SQLite Version Error");
      message.AppendASCII("The application has been updated, but your version "
                          "of SQLite is too old and the application cannot "
                          "run.");
      (void)ps->Alert(nsnull, title.get(), message.get());
    }
    ::PR_Abort();
  }

  gService = new Service();
  if (gService) {
    NS_ADDREF(gService);
    if (NS_FAILED(gService->initialize()))
      NS_RELEASE(gService);
  }

  return gService;
}

nsIXPConnect *Service::sXPConnect = nsnull;

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

PRInt32 Service::sSynchronousPref;

// static
PRInt32
Service::getSynchronousPref()
{
  return sSynchronousPref;
}

Service::Service()
: mMutex("Service::mMutex")
, mSqliteVFS(nsnull)
, mRegistrationMutex("Service::mRegistrationMutex")
, mConnections()
, mStorageSQLiteReporter(nsnull)
, mStorageSQLiteMultiReporter(nsnull)
{
}

Service::~Service()
{
  (void)::NS_UnregisterMemoryReporter(mStorageSQLiteReporter);
  (void)::NS_UnregisterMemoryMultiReporter(mStorageSQLiteMultiReporter);

  int rc = sqlite3_vfs_unregister(mSqliteVFS);
  if (rc != SQLITE_OK)
    NS_WARNING("Failed to unregister sqlite vfs wrapper.");

  // Shutdown the sqlite3 API.  Warn if shutdown did not turn out okay, but
  // there is nothing actionable we can do in that case.
  rc = ::sqlite3_quota_shutdown();
  if (rc != SQLITE_OK)
    NS_WARNING("sqlite3 did not shutdown cleanly.");

  rc = ::sqlite3_shutdown();
  if (rc != SQLITE_OK)
    NS_WARNING("sqlite3 did not shutdown cleanly.");

  DebugOnly<bool> shutdownObserved = !sXPConnect;
  NS_ASSERTION(shutdownObserved, "Shutdown was not observed!");

  gService = nsnull;
  delete mSqliteVFS;
  mSqliteVFS = nsnull;
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
  nsRefPtr<Service> kungFuDeathGrip(this);
  {
    mRegistrationMutex.AssertNotCurrentThreadOwns();
    MutexAutoLock mutex(mRegistrationMutex);
    DebugOnly<bool> removed = mConnections.RemoveElement(aConnection);
    // Assert if we try to unregister a non-existent connection.
    MOZ_ASSERT(removed);
  }
}

void
Service::getConnections(/* inout */ nsTArray<nsRefPtr<Connection> >& aConnections)
{
  mRegistrationMutex.AssertNotCurrentThreadOwns();
  MutexAutoLock mutex(mRegistrationMutex);
  aConnections.Clear();
  aConnections.AppendElements(mConnections);
}

void
Service::shutdown()
{
  NS_IF_RELEASE(sXPConnect);
}

sqlite3_vfs *ConstructTelemetryVFS();

#ifdef MOZ_MEMORY

#  if defined(XP_WIN) || defined(SOLARIS) || defined(ANDROID) || defined(XP_MACOSX)
#    include "jemalloc.h"
#  elif defined(XP_LINUX)
// jemalloc is directly linked into firefox-bin; libxul doesn't link
// with it.  So if we tried to use je_malloc_usable_size_in_advance directly
// here, it wouldn't be defined.  Instead, we don't include the jemalloc header
// and weakly link against je_malloc_usable_size_in_advance.
extern "C" {
extern size_t je_malloc_usable_size_in_advance(size_t size)
  NS_VISIBILITY_DEFAULT __attribute__((weak));
}
#  endif  // XP_LINUX

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
// Note: the wrappers for moz_malloc, moz_realloc and moz_malloc_usable_size
// are necessary because the sqlite_mem_methods type signatures differ slightly
// from the standard ones -- they use int instead of size_t.  But we don't need
// a wrapper for moz_free.

static void *sqliteMemMalloc(int n)
{
  return ::moz_malloc(n);
}

static void *sqliteMemRealloc(void *p, int n)
{
  return ::moz_realloc(p, n);
}

static int sqliteMemSize(void *p)
{
  return ::moz_malloc_usable_size(p);
}

static int sqliteMemRoundup(int n)
{
  n = je_malloc_usable_size_in_advance(n);

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
  &moz_free,
  &sqliteMemRealloc,
  &sqliteMemSize,
  &sqliteMemRoundup,
  &sqliteMemInit,
  &sqliteMemShutdown,
  NULL
}; 

} // anonymous namespace

#endif  // MOZ_MEMORY

nsresult
Service::initialize()
{
  NS_TIME_FUNCTION;

  int rc;

#ifdef MOZ_MEMORY
  rc = ::sqlite3_config(SQLITE_CONFIG_MALLOC, &memMethods);
  if (rc != SQLITE_OK)
    return convertResultCode(rc);
#endif

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
  rc = ::sqlite3_quota_initialize("telemetry-vfs", 0);
  if (rc != SQLITE_OK)
    return convertResultCode(rc);

  // Set the default value for the toolkit.storage.synchronous pref.  It will be
  // updated with the user preference on the main thread.
  sSynchronousPref = PREF_TS_SYNCHRONOUS_DEFAULT;

  // Run the things that need to run on the main thread there.
  nsCOMPtr<nsIRunnable> event =
    new ServiceMainThreadInitializer(this, this, &sXPConnect, &sSynchronousPref);
  if (event && ::NS_IsMainThread()) {
    (void)event->Run();
  }
  else {
    (void)::NS_DispatchToMainThread(event);
  }

  return NS_OK;
}

int
Service::localeCompareStrings(const nsAString &aStr1,
                              const nsAString &aStr2,
                              PRInt32 aComparisonStrength)
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

  PRInt32 res;
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
    return nsnull;
  }

  nsCOMPtr<nsILocale> appLocale;
  nsresult rv = svc->GetApplicationLocale(getter_AddRefs(appLocale));
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not get application locale");
    return nsnull;
  }

  nsCOMPtr<nsICollationFactory> collFact =
    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  if (!collFact) {
    NS_WARNING("Could not create collation factory");
    return nsnull;
  }

  rv = collFact->CreateCollation(appLocale, getter_AddRefs(mLocaleCollation));
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not create collation");
    return nsnull;
  }

  return mLocaleCollation;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageService

#ifndef NS_APP_STORAGE_50_FILE
#define NS_APP_STORAGE_50_FILE "UStor"
#endif

NS_IMETHODIMP
Service::OpenSpecialDatabase(const char *aStorageKey,
                             mozIStorageConnection **_connection)
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageFile;
  if (::strcmp(aStorageKey, "memory") == 0) {
    // just fall through with NULL storageFile, this will cause the storage
    // connection to use a memory DB.
  }
  else if (::strcmp(aStorageKey, "profile") == 0) {

    rv = NS_GetSpecialDirectory(NS_APP_STORAGE_50_FILE,
                                getter_AddRefs(storageFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsString filename;
    storageFile->GetPath(filename);
    nsCString filename8 = NS_ConvertUTF16toUTF8(filename.get());
    // fall through to DB initialization
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  Connection *msc = new Connection(this, SQLITE_OPEN_READWRITE);
  NS_ENSURE_TRUE(msc, NS_ERROR_OUT_OF_MEMORY);

  rv = msc->initialize(storageFile);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_connection = msc);
  return NS_OK;
}

NS_IMETHODIMP
Service::OpenDatabase(nsIFile *aDatabaseFile,
                      mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aDatabaseFile);

#ifdef NS_FUNCTION_TIMER
  nsCString leafname;
  (void)aDatabaseFile->GetNativeLeafName(leafname);
  NS_TIME_FUNCTION_FMT("mozIStorageService::OpenDatabase(%s)", leafname.get());
#endif

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE |
              SQLITE_OPEN_CREATE;
  nsRefPtr<Connection> msc = new Connection(this, flags);
  NS_ENSURE_TRUE(msc, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = msc->initialize(aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_connection = msc);
  return NS_OK;
}

NS_IMETHODIMP
Service::OpenUnsharedDatabase(nsIFile *aDatabaseFile,
                              mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aDatabaseFile);

#ifdef NS_FUNCTION_TIMER
  nsCString leafname;
  (void)aDatabaseFile->GetNativeLeafName(leafname);
  NS_TIME_FUNCTION_FMT("mozIStorageService::OpenUnsharedDatabase(%s)",
                       leafname.get());
#endif

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_PRIVATECACHE |
              SQLITE_OPEN_CREATE;
  nsRefPtr<Connection> msc = new Connection(this, flags);
  NS_ENSURE_TRUE(msc, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = msc->initialize(aDatabaseFile);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_connection = msc);
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
Service::Observe(nsISupports *, const char *aTopic, const PRUnichar *)
{
  if (strcmp(aTopic, "xpcom-shutdown") == 0)
    shutdown();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageServiceQuotaManagement

NS_IMETHODIMP
Service::OpenDatabaseWithVFS(nsIFile *aDatabaseFile,
                             const nsACString &aVFSName,
                             mozIStorageConnection **_connection)
{
  NS_ENSURE_ARG(aDatabaseFile);

#ifdef NS_FUNCTION_TIMER
  nsCString leafname;
  (void)aDatabaseFile->GetNativeLeafName(leafname);
  NS_TIME_FUNCTION_FMT("mozIStorageService::OpenDatabaseWithVFS(%s)",
                       leafname.get());
#endif

  // Always ensure that SQLITE_OPEN_CREATE is passed in for compatibility
  // reasons.
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_SHAREDCACHE |
              SQLITE_OPEN_CREATE;
  nsRefPtr<Connection> msc = new Connection(this, flags);
  NS_ENSURE_TRUE(msc, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = msc->initialize(aDatabaseFile,
                                PromiseFlatCString(aVFSName).get());
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_connection = msc);
  return NS_OK;
}

NS_IMETHODIMP
Service::SetQuotaForFilenamePattern(const nsACString &aPattern,
                                    PRInt64 aSizeLimit,
                                    mozIStorageQuotaCallback *aCallback,
                                    nsISupports *aUserData)
{
  NS_ENSURE_FALSE(aPattern.IsEmpty(), NS_ERROR_INVALID_ARG);

  nsAutoPtr<QuotaCallbackData> data;
  if (aSizeLimit && aCallback) {
    data = new QuotaCallbackData(aCallback, aUserData);
  }

  int rc = ::sqlite3_quota_set(PromiseFlatCString(aPattern).get(),
                               aSizeLimit, QuotaCallbackData::Callback,
                               data, QuotaCallbackData::Destroy);
  NS_ENSURE_TRUE(rc == SQLITE_OK, convertResultCode(rc));

  data.forget();
  return NS_OK;
}

NS_IMETHODIMP
Service::UpdateQuotaInformationForFile(nsIFile *aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  nsCString path;
  nsresult rv = aFile->GetNativePath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  int rc = ::sqlite3_quota_file(PromiseFlatCString(path).get());
  NS_ENSURE_TRUE(rc == SQLITE_OK, convertResultCode(rc));

  return NS_OK;
}

} // namespace storage
} // namespace mozilla
