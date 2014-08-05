/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "Predictor.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsICancelable.h"
#include "nsIChannel.h"
#include "nsIDNSListener.h"
#include "nsIDNSService.h"
#include "nsIDocument.h"
#include "nsIFile.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsINetworkPredictorVerifier.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsISpeculativeConnect.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif
#include "prlog.h"

#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageHelper.h"

#include "mozilla/Preferences.h"
#include "mozilla/storage.h"
#include "mozilla/Telemetry.h"

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
#include "nsIPropertyBag2.h"
static const int32_t ANDROID_23_VERSION = 10;
#endif

using namespace mozilla;

namespace mozilla {
namespace net {

#define RETURN_IF_FAILED(_rv) \
  do { \
    if (NS_FAILED(_rv)) { \
      return; \
    } \
  } while (0)

const char PREDICTOR_ENABLED_PREF[] = "network.predictor.enabled";
const char PREDICTOR_SSL_HOVER_PREF[] = "network.predictor.enable-hover-on-ssl";

const char PREDICTOR_PAGE_DELTA_DAY_PREF[] =
  "network.predictor.page-degradation.day";
const int PREDICTOR_PAGE_DELTA_DAY_DEFAULT = 0;
const char PREDICTOR_PAGE_DELTA_WEEK_PREF[] =
  "network.predictor.page-degradation.week";
const int PREDICTOR_PAGE_DELTA_WEEK_DEFAULT = 5;
const char PREDICTOR_PAGE_DELTA_MONTH_PREF[] =
  "network.predictor.page-degradation.month";
const int PREDICTOR_PAGE_DELTA_MONTH_DEFAULT = 10;
const char PREDICTOR_PAGE_DELTA_YEAR_PREF[] =
  "network.predictor.page-degradation.year";
const int PREDICTOR_PAGE_DELTA_YEAR_DEFAULT = 25;
const char PREDICTOR_PAGE_DELTA_MAX_PREF[] =
  "network.predictor.page-degradation.max";
const int PREDICTOR_PAGE_DELTA_MAX_DEFAULT = 50;
const char PREDICTOR_SUB_DELTA_DAY_PREF[] =
  "network.predictor.subresource-degradation.day";
const int PREDICTOR_SUB_DELTA_DAY_DEFAULT = 1;
const char PREDICTOR_SUB_DELTA_WEEK_PREF[] =
  "network.predictor.subresource-degradation.week";
const int PREDICTOR_SUB_DELTA_WEEK_DEFAULT = 10;
const char PREDICTOR_SUB_DELTA_MONTH_PREF[] =
  "network.predictor.subresource-degradation.month";
const int PREDICTOR_SUB_DELTA_MONTH_DEFAULT = 25;
const char PREDICTOR_SUB_DELTA_YEAR_PREF[] =
  "network.predictor.subresource-degradation.year";
const int PREDICTOR_SUB_DELTA_YEAR_DEFAULT = 50;
const char PREDICTOR_SUB_DELTA_MAX_PREF[] =
  "network.predictor.subresource-degradation.max";
const int PREDICTOR_SUB_DELTA_MAX_DEFAULT = 100;

const char PREDICTOR_PRECONNECT_MIN_PREF[] =
  "network.predictor.preconnect-min-confidence";
const int PRECONNECT_MIN_DEFAULT = 90;
const char PREDICTOR_PRERESOLVE_MIN_PREF[] =
  "network.predictor.preresolve-min-confidence";
const int PRERESOLVE_MIN_DEFAULT = 60;
const char PREDICTOR_REDIRECT_LIKELY_PREF[] =
  "network.predictor.redirect-likely-confidence";
const int REDIRECT_LIKELY_DEFAULT = 75;

const char PREDICTOR_MAX_QUEUE_SIZE_PREF[] = "network.predictor.max-queue-size";
const uint32_t PREDICTOR_MAX_QUEUE_SIZE_DEFAULT = 50;

const char PREDICTOR_MAX_DB_SIZE_PREF[] = "network.predictor.max-db-size";
const int32_t PREDICTOR_MAX_DB_SIZE_DEFAULT_BYTES = 150 * 1024 * 1024;
const char PREDICTOR_PRESERVE_PERCENTAGE_PREF[] = "network.predictor.preserve";
const int32_t PREDICTOR_PRESERVE_PERCENTAGE_DEFAULT = 80;

// All these time values are in usec
const long long ONE_DAY = 86400LL * 1000000LL;
const long long ONE_WEEK = 7LL * ONE_DAY;
const long long ONE_MONTH = 30LL * ONE_DAY;
const long long ONE_YEAR = 365LL * ONE_DAY;

const long STARTUP_WINDOW = 5L * 60L * 1000000L; // 5min

// Version for the database schema
static const int32_t PREDICTOR_SCHEMA_VERSION = 1;

// Listener for the speculative DNS requests we'll fire off, which just ignores
// the result (since we're just trying to warm the cache). This also exists to
// reduce round-trips to the main thread, by being something threadsafe the
// Predictor can use.

class PredictorDNSListener : public nsIDNSListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  PredictorDNSListener()
  { }

private:
  virtual ~PredictorDNSListener()
  { }
};

NS_IMPL_ISUPPORTS(PredictorDNSListener, nsIDNSListener);

NS_IMETHODIMP
PredictorDNSListener::OnLookupComplete(nsICancelable *request,
                                       nsIDNSRecord *rec,
                                       nsresult status)
{
  return NS_OK;
}

// Are you ready for the fun part? Because here comes the fun part. The
// predictor, which will do awesome stuff as you browse to make your
// browsing experience faster.

static Predictor *gPredictor = nullptr;

#if defined(PR_LOGGING)
static PRLogModuleInfo *gPredictorLog = nullptr;
#define PREDICTOR_LOG(args) PR_LOG(gPredictorLog, 4, args)
#else
#define PREDICTOR_LOG(args)
#endif

NS_IMPL_ISUPPORTS(Predictor,
                  nsINetworkPredictor,
                  nsIObserver,
                  nsISpeculativeConnectionOverrider,
                  nsIInterfaceRequestor)

Predictor::Predictor()
  :mInitialized(false)
  ,mEnabled(true)
  ,mEnableHoverOnSSL(false)
  ,mPageDegradationDay(PREDICTOR_PAGE_DELTA_DAY_DEFAULT)
  ,mPageDegradationWeek(PREDICTOR_PAGE_DELTA_WEEK_DEFAULT)
  ,mPageDegradationMonth(PREDICTOR_PAGE_DELTA_MONTH_DEFAULT)
  ,mPageDegradationYear(PREDICTOR_PAGE_DELTA_YEAR_DEFAULT)
  ,mPageDegradationMax(PREDICTOR_PAGE_DELTA_MAX_DEFAULT)
  ,mSubresourceDegradationDay(PREDICTOR_SUB_DELTA_DAY_DEFAULT)
  ,mSubresourceDegradationWeek(PREDICTOR_SUB_DELTA_WEEK_DEFAULT)
  ,mSubresourceDegradationMonth(PREDICTOR_SUB_DELTA_MONTH_DEFAULT)
  ,mSubresourceDegradationYear(PREDICTOR_SUB_DELTA_YEAR_DEFAULT)
  ,mSubresourceDegradationMax(PREDICTOR_SUB_DELTA_MAX_DEFAULT)
  ,mPreconnectMinConfidence(PRECONNECT_MIN_DEFAULT)
  ,mPreresolveMinConfidence(PRERESOLVE_MIN_DEFAULT)
  ,mRedirectLikelyConfidence(REDIRECT_LIKELY_DEFAULT)
  ,mMaxQueueSize(PREDICTOR_MAX_QUEUE_SIZE_DEFAULT)
  ,mStatements(mDB)
  ,mLastStartupTime(0)
  ,mStartupCount(0)
  ,mQueueSize(0)
  ,mQueueSizeLock("Predictor.mQueueSizeLock")
  ,mCleanupScheduled(false)
  ,mMaxDBSize(PREDICTOR_MAX_DB_SIZE_DEFAULT_BYTES)
  ,mPreservePercentage(PREDICTOR_PRESERVE_PERCENTAGE_DEFAULT)
  ,mLastCleanupTime(0)
{
#if defined(PR_LOGGING)
  gPredictorLog = PR_NewLogModule("NetworkPredictor");
#endif

  MOZ_ASSERT(!gPredictor, "multiple Predictor instances!");
  gPredictor = this;
}

Predictor::~Predictor()
{
  if (mInitialized)
    Shutdown();

  RemoveObserver();

  gPredictor = nullptr;
}

// Predictor::nsIObserver

nsresult
Predictor::InstallObserver()
{
  MOZ_ASSERT(NS_IsMainThread(), "Installing observer off main thread");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Preferences::AddBoolVarCache(&mEnabled, PREDICTOR_ENABLED_PREF, true);
  Preferences::AddBoolVarCache(&mEnableHoverOnSSL,
                               PREDICTOR_SSL_HOVER_PREF, false);
  Preferences::AddIntVarCache(&mPageDegradationDay,
                              PREDICTOR_PAGE_DELTA_DAY_PREF,
                              PREDICTOR_PAGE_DELTA_DAY_DEFAULT);
  Preferences::AddIntVarCache(&mPageDegradationWeek,
                              PREDICTOR_PAGE_DELTA_WEEK_PREF,
                              PREDICTOR_PAGE_DELTA_WEEK_DEFAULT);
  Preferences::AddIntVarCache(&mPageDegradationMonth,
                              PREDICTOR_PAGE_DELTA_MONTH_PREF,
                              PREDICTOR_PAGE_DELTA_MONTH_DEFAULT);
  Preferences::AddIntVarCache(&mPageDegradationYear,
                              PREDICTOR_PAGE_DELTA_YEAR_PREF,
                              PREDICTOR_PAGE_DELTA_YEAR_DEFAULT);
  Preferences::AddIntVarCache(&mPageDegradationMax,
                              PREDICTOR_PAGE_DELTA_MAX_PREF,
                              PREDICTOR_PAGE_DELTA_MAX_DEFAULT);

  Preferences::AddIntVarCache(&mSubresourceDegradationDay,
                              PREDICTOR_SUB_DELTA_DAY_PREF,
                              PREDICTOR_SUB_DELTA_DAY_DEFAULT);
  Preferences::AddIntVarCache(&mSubresourceDegradationWeek,
                              PREDICTOR_SUB_DELTA_WEEK_PREF,
                              PREDICTOR_SUB_DELTA_WEEK_DEFAULT);
  Preferences::AddIntVarCache(&mSubresourceDegradationMonth,
                              PREDICTOR_SUB_DELTA_MONTH_PREF,
                              PREDICTOR_SUB_DELTA_MONTH_DEFAULT);
  Preferences::AddIntVarCache(&mSubresourceDegradationYear,
                              PREDICTOR_SUB_DELTA_YEAR_PREF,
                              PREDICTOR_SUB_DELTA_YEAR_DEFAULT);
  Preferences::AddIntVarCache(&mSubresourceDegradationMax,
                              PREDICTOR_SUB_DELTA_MAX_PREF,
                              PREDICTOR_SUB_DELTA_MAX_DEFAULT);

  Preferences::AddIntVarCache(&mPreconnectMinConfidence,
                              PREDICTOR_PRECONNECT_MIN_PREF,
                              PRECONNECT_MIN_DEFAULT);
  Preferences::AddIntVarCache(&mPreresolveMinConfidence,
                              PREDICTOR_PRERESOLVE_MIN_PREF,
                              PRERESOLVE_MIN_DEFAULT);
  Preferences::AddIntVarCache(&mRedirectLikelyConfidence,
                              PREDICTOR_REDIRECT_LIKELY_PREF,
                              REDIRECT_LIKELY_DEFAULT);

  Preferences::AddIntVarCache(&mMaxQueueSize, PREDICTOR_MAX_QUEUE_SIZE_PREF,
                              PREDICTOR_MAX_QUEUE_SIZE_DEFAULT);

  Preferences::AddIntVarCache(&mMaxDBSize, PREDICTOR_MAX_DB_SIZE_PREF,
                              PREDICTOR_MAX_DB_SIZE_DEFAULT_BYTES);
  Preferences::AddIntVarCache(&mPreservePercentage,
                              PREDICTOR_PRESERVE_PERCENTAGE_PREF,
                              PREDICTOR_PRESERVE_PERCENTAGE_DEFAULT);

  return rv;
}

void
Predictor::RemoveObserver()
{
  MOZ_ASSERT(NS_IsMainThread(), "Removing observer off main thread");

  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

static const uint32_t COMMIT_TIMER_DELTA_MS = 5 * 1000;

class PredictorCommitTimerInitEvent : public nsRunnable
{
public:
  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    nsresult rv = NS_OK;

    if (!gPredictor->mCommitTimer) {
      gPredictor->mCommitTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    } else {
      gPredictor->mCommitTimer->Cancel();
    }
    if (NS_SUCCEEDED(rv)) {
      gPredictor->mCommitTimer->Init(gPredictor, COMMIT_TIMER_DELTA_MS,
                                     nsITimer::TYPE_ONE_SHOT);
    }

    return NS_OK;
  }
};

class PredictorNewTransactionEvent : public nsRunnable
{
  NS_IMETHODIMP Run() MOZ_OVERRIDE
  {
    gPredictor->CommitTransaction();
    gPredictor->BeginTransaction();
    gPredictor->MaybeScheduleCleanup();
    nsRefPtr<PredictorCommitTimerInitEvent> event =
      new PredictorCommitTimerInitEvent();
    NS_DispatchToMainThread(event);
    return NS_OK;
  }
};

NS_IMETHODIMP
Predictor::Observe(nsISupports *subject, const char *topic,
                   const char16_t *data_unicode)
{
  nsresult rv = NS_OK;
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor observing something off main thread!");

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    Shutdown();
  } else if (!strcmp(NS_TIMER_CALLBACK_TOPIC, topic)) {
    if (mInitialized) { // Can't access io thread if we're not initialized!
      nsRefPtr<PredictorNewTransactionEvent> event =
        new PredictorNewTransactionEvent();
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }

  return rv;
}

// Predictor::nsISpeculativeConnectionOverrider

NS_IMETHODIMP
Predictor::GetIgnoreIdle(bool *ignoreIdle)
{
  *ignoreIdle = true;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetIgnorePossibleSpdyConnections(bool *ignorePossibleSpdyConnections)
{
  *ignorePossibleSpdyConnections = true;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetParallelSpeculativeConnectLimit(
    uint32_t *parallelSpeculativeConnectLimit)
{
  *parallelSpeculativeConnectLimit = 6;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetIsFromPredictor(bool *isFromPredictor)
{
  *isFromPredictor = true;
  return NS_OK;
}

// Predictor::nsIInterfaceRequestor

NS_IMETHODIMP
Predictor::GetInterface(const nsIID &iid, void **result)
{
  return QueryInterface(iid, result);
}

#ifdef MOZ_NUWA_PROCESS
class NuwaMarkPredictorThreadRunner : public nsRunnable
{
  NS_IMETHODIMP Run() MOZ_OVERRIDE
  {
    if (IsNuwaProcess()) {
      NS_ASSERTION(NuwaMarkCurrentThread != nullptr,
                   "NuwaMarkCurrentThread is undefined!");
      NuwaMarkCurrentThread(nullptr, nullptr);
    }
    return NS_OK;
  }
};
#endif

// Predictor::nsINetworkPredictor

nsresult
Predictor::Init()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "Predictor::Init called off the main thread!");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
  // This is an ugly hack to disable the predictor on android < 2.3, as it
  // doesn't play nicely with those android versions, at least on our infra.
  // Causes timeouts in reftests. See bug 881804 comment 86.
  nsCOMPtr<nsIPropertyBag2> infoService =
    do_GetService("@mozilla.org/system-info;1");
  if (infoService) {
    int32_t androidVersion = -1;
    rv = infoService->GetPropertyAsInt32(NS_LITERAL_STRING("version"),
                                         &androidVersion);
    if (NS_SUCCEEDED(rv) && (androidVersion < ANDROID_23_VERSION)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
#endif

  mStartupTime = PR_Now();

  rv = InstallObserver();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mDNSListener) {
    mDNSListener = new PredictorDNSListener();
  }

  rv = NS_NewNamedThread("Net Predictor", getter_AddRefs(mIOThread));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_NUWA_PROCESS
  nsCOMPtr<nsIRunnable> runner = new NuwaMarkPredictorThreadRunner();
  mIOThread->Dispatch(runner, NS_DISPATCH_NORMAL);
#endif

  mSpeculativeService = do_GetService("@mozilla.org/network/io-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mStorageService = do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(mDBFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBFile->AppendNative(NS_LITERAL_CSTRING("netpredictions.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = true;

  return rv;
}

void
Predictor::CheckForAndDeleteOldDBFile()
{
  nsCOMPtr<nsIFile> oldDBFile;
  nsresult rv = mDBFile->GetParent(getter_AddRefs(oldDBFile));
  RETURN_IF_FAILED(rv);

  rv = oldDBFile->AppendNative(NS_LITERAL_CSTRING("seer.sqlite"));
  RETURN_IF_FAILED(rv);

  bool oldFileExists = false;
  rv = oldDBFile->Exists(&oldFileExists);
  if (NS_FAILED(rv) || !oldFileExists) {
    return;
  }

  oldDBFile->Remove(false);
}

// Make sure that our sqlite storage is all set up with all the tables we need
// to do the work. It isn't the end of the world if this fails, since this is
// all an optimization, anyway.

nsresult
Predictor::EnsureInitStorage()
{
  MOZ_ASSERT(!NS_IsMainThread(),
             "Initializing predictor storage on main thread");

  if (mDB) {
    return NS_OK;
  }

  nsresult rv;

  CheckForAndDeleteOldDBFile();

  rv = mStorageService->OpenDatabase(mDBFile, getter_AddRefs(mDB));
  if (NS_FAILED(rv)) {
    // Retry once by trashing the file and trying to open again. If this fails,
    // we can just bail, and hope for better luck next time.
    rv = mDBFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStorageService->OpenDatabase(mDBFile, getter_AddRefs(mDB));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF;"));
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA foreign_keys = ON;"));

  BeginTransaction();

  // A table to make sure we're working with the database layout we expect
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_predictor_version (\n"
                         "  version INTEGER NOT NULL\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDB->CreateStatement(
      NS_LITERAL_CSTRING("SELECT version FROM moz_predictor_version;\n"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasRows;
  rv = stmt->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasRows) {
    int32_t currentVersion;
    rv = stmt->GetInt32(0, &currentVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is what we do while we only have one schema version. Later, we'll
    // have to change this to actually upgrade things as appropriate.
    MOZ_ASSERT(currentVersion == PREDICTOR_SCHEMA_VERSION,
               "Invalid predictor schema version!");
    if (currentVersion != PREDICTOR_SCHEMA_VERSION) {
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    stmt = nullptr;
    rv = mDB->CreateStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_predictor_version (version) VALUES "
                           "(:predictor_version);"),
        getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("predictor_version"),
                               PREDICTOR_SCHEMA_VERSION);
    NS_ENSURE_SUCCESS(rv, rv);

    stmt->Execute();
  }

  stmt = nullptr;

  // This table keeps track of the hosts we've seen at the top level of a
  // pageload so we can map them to hosts used for subresources of a pageload.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_hosts (\n"
                         "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                         "  origin TEXT NOT NULL,\n"
                         "  loads INTEGER DEFAULT 0,\n"
                         "  last_load INTEGER DEFAULT 0\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS host_id_origin_index "
                         "ON moz_hosts (id, origin);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS host_origin_index "
                         "ON moz_hosts (origin);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS host_load_index "
                         "ON moz_hosts (last_load);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // And this is the table that keeps track of the hosts for subresources of a
  // pageload.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_subhosts (\n"
                         "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                         "  hid INTEGER NOT NULL,\n"
                         "  origin TEXT NOT NULL,\n"
                         "  hits INTEGER DEFAULT 0,\n"
                         "  last_hit INTEGER DEFAULT 0,\n"
                         "  FOREIGN KEY(hid) REFERENCES moz_hosts(id) ON DELETE CASCADE\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS subhost_hid_origin_index "
                         "ON moz_subhosts (hid, origin);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS subhost_id_index "
                         "ON moz_subhosts (id);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Table to keep track of how many times we've started up, and when the last
  // time was.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_startups (\n"
                         "  startups INTEGER,\n"
                         "  last_startup INTEGER\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->CreateStatement(
      NS_LITERAL_CSTRING("SELECT startups, last_startup FROM moz_startups;\n"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  // We'll go ahead and keep track of our startup count here, since we can
  // (mostly) equate "the service was created and asked to do stuff" with
  // "the browser was started up".
  rv = stmt->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasRows) {
    // We've started up before. Update our startup statistics
    stmt->GetInt32(0, &mStartupCount);
    stmt->GetInt64(1, &mLastStartupTime);

    // This finalizes the statement
    stmt = nullptr;

    rv = mDB->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE moz_startups SET startups = :startup_count, "
                           "last_startup = :startup_time;\n"),
        getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t newStartupCount = mStartupCount + 1;
    if (newStartupCount <= 0) {
      PREDICTOR_LOG(("Predictor::EnsureInitStorage startup count overflow\n"));
      newStartupCount = mStartupCount;
      Telemetry::AutoCounter<Telemetry::PREDICTOR_STARTUP_COUNT_OVERFLOWS> startupCountOverflows;
      ++startupCountOverflows;
    }

    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("startup_count"),
                               mStartupCount + 1);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("startup_time"),
                               mStartupTime);
    NS_ENSURE_SUCCESS(rv, rv);

    stmt->Execute();
  } else {
    // This is our first startup, so let's go ahead and mark it as such
    mStartupCount = 1;

    rv = mDB->CreateStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_startups (startups, last_startup) "
                           "VALUES (1, :startup_time);\n"),
        getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("startup_time"),
                               mStartupTime);
    NS_ENSURE_SUCCESS(rv, rv);

    stmt->Execute();
  }

  // This finalizes the statement
  stmt = nullptr;

  // This table lists URIs loaded at startup, along with how many startups
  // they've been loaded during, and when the last time was.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_startup_pages (\n"
                         "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                         "  uri TEXT NOT NULL,\n"
                         "  hits INTEGER DEFAULT 0,\n"
                         "  last_hit INTEGER DEFAULT 0\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS startup_page_uri_index "
                         "ON moz_startup_pages (uri);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS startup_page_hit_index "
                         "ON moz_startup_pages (last_hit);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // This table is similar to moz_hosts above, but uses full URIs instead of
  // hosts so that we can get more specific predictions for URIs that people
  // visit often (such as their email or social network home pages).
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_pages (\n"
                         "  id integer PRIMARY KEY AUTOINCREMENT,\n"
                         "  uri TEXT NOT NULL,\n"
                         "  loads INTEGER DEFAULT 0,\n"
                         "  last_load INTEGER DEFAULT 0\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS page_id_uri_index "
                         "ON moz_pages (id, uri);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS page_uri_index "
                         "ON moz_pages (uri);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // This table is similar to moz_subhosts above, but is instead related to
  // moz_pages for finer granularity.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_subresources (\n"
                         "  id integer PRIMARY KEY AUTOINCREMENT,\n"
                         "  pid INTEGER NOT NULL,\n"
                         "  uri TEXT NOT NULL,\n"
                         "  hits INTEGER DEFAULT 0,\n"
                         "  last_hit INTEGER DEFAULT 0,\n"
                         "  FOREIGN KEY(pid) REFERENCES moz_pages(id) ON DELETE CASCADE\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS subresource_pid_uri_index "
                         "ON moz_subresources (pid, uri);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS subresource_id_index "
                         "ON moz_subresources (id);"));
  NS_ENSURE_SUCCESS(rv, rv);

  // This table keeps track of URIs and what they end up finally redirecting to
  // so we can handle redirects in a sane fashion, as well.
  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS moz_redirects (\n"
                         "  id integer PRIMARY KEY AUTOINCREMENT,\n"
                         "  pid integer NOT NULL,\n"
                         "  uri TEXT NOT NULL,\n"
                         "  origin TEXT NOT NULL,\n"
                         "  hits INTEGER DEFAULT 0,\n"
                         "  last_hit INTEGER DEFAULT 0,\n"
                         "  FOREIGN KEY(pid) REFERENCES moz_pages(id) ON DELETE CASCADE\n"
                         ");\n"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS redirect_pid_uri_index "
                         "ON moz_redirects (pid, uri);"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDB->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX IF NOT EXISTS redirect_id_index "
                         "ON moz_redirects (id);"));
  NS_ENSURE_SUCCESS(rv, rv);

  CommitTransaction();
  BeginTransaction();

  nsRefPtr<PredictorCommitTimerInitEvent> event =
    new PredictorCommitTimerInitEvent();
  NS_DispatchToMainThread(event);

  return NS_OK;
}

class PredictorThreadShutdownRunner : public nsRunnable
{
public:
  explicit PredictorThreadShutdownRunner(nsIThread *ioThread)
    :mIOThread(ioThread)
  { }

  NS_IMETHODIMP Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread(),
               "Shut down predictor io thread off main thread");
    mIOThread->Shutdown();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mIOThread;
};

class PredictorDBShutdownRunner : public nsRunnable
{
public:
  PredictorDBShutdownRunner(nsIThread *ioThread, nsINetworkPredictor *predictor)
    :mIOThread(ioThread)
  {
    mPredictor = new nsMainThreadPtrHolder<nsINetworkPredictor>(predictor);
  }

  NS_IMETHODIMP Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Shutting down DB on main thread");

    // Ensure everything is written to disk before we shut down the db
    gPredictor->CommitTransaction();

    gPredictor->mStatements.FinalizeStatements();
    gPredictor->mDB->Close();
    gPredictor->mDB = nullptr;

    nsRefPtr<PredictorThreadShutdownRunner> runner =
      new PredictorThreadShutdownRunner(mIOThread);
    NS_DispatchToMainThread(runner);

    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mIOThread;

  // Death grip to keep predictor alive while we cleanly close its DB connection
  nsMainThreadPtrHandle<nsINetworkPredictor> mPredictor;
};

void
Predictor::Shutdown()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "Predictor::Shutdown called off the main thread!");
    return;
  }

  mInitialized = false;

  if (mCommitTimer) {
    mCommitTimer->Cancel();
  }

  if (mIOThread) {
    if (mDB) {
      nsRefPtr<PredictorDBShutdownRunner> runner =
        new PredictorDBShutdownRunner(mIOThread, this);
      mIOThread->Dispatch(runner, NS_DISPATCH_NORMAL);
    } else {
      nsRefPtr<PredictorThreadShutdownRunner> runner =
        new PredictorThreadShutdownRunner(mIOThread);
      NS_DispatchToMainThread(runner);
    }
  }
}

nsresult
Predictor::Create(nsISupports *aOuter, const nsIID& aIID,
                  void **aResult)
{
  nsresult rv;

  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsRefPtr<Predictor> svc = new Predictor();

  rv = svc->Init();
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(("Failed to initialize predictor, predictor will be a noop"));
  }

  // We treat init failure the same as the service being disabled, since this
  // is all an optimization anyway. No need to freak people out. That's why we
  // gladly continue on QI'ing here.
  rv = svc->QueryInterface(aIID, aResult);

  return rv;
}

// Get the full origin (scheme, host, port) out of a URI (maybe should be part
// of nsIURI instead?)
static void
ExtractOrigin(nsIURI *uri, nsAutoCString &s)
{
  s.Truncate();

  nsAutoCString scheme;
  nsresult rv = uri->GetScheme(scheme);
  RETURN_IF_FAILED(rv);

  nsAutoCString host;
  rv = uri->GetAsciiHost(host);
  RETURN_IF_FAILED(rv);

  int32_t port;
  rv = uri->GetPort(&port);
  RETURN_IF_FAILED(rv);

  s.Assign(scheme);
  s.AppendLiteral("://");
  s.Append(host);
  if (port != -1) {
    s.Append(':');
    s.AppendInt(port);
  }
}

// An event to do the work for a prediction that needs to hit the sqlite
// database. These events should be created on the main thread, and run on
// the predictor thread.
class PredictionEvent : public nsRunnable
{
public:
  PredictionEvent(nsIURI *targetURI, nsIURI *sourceURI,
                  PredictorPredictReason reason,
                  nsINetworkPredictorVerifier *verifier)
    :mReason(reason)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Creating prediction event off main thread");

    mEnqueueTime = TimeStamp::Now();

    if (verifier) {
      mVerifier =
        new nsMainThreadPtrHolder<nsINetworkPredictorVerifier>(verifier);
    }
    if (targetURI) {
      targetURI->GetAsciiSpec(mTargetURI.spec);
      ExtractOrigin(targetURI, mTargetURI.origin);
    }
    if (sourceURI) {
      sourceURI->GetAsciiSpec(mSourceURI.spec);
      ExtractOrigin(sourceURI, mSourceURI.origin);
    }
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Running prediction event on main thread");

    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_PREDICT_WAIT_TIME,
                                   mEnqueueTime);

    TimeStamp startTime = TimeStamp::Now();

    nsresult rv = NS_OK;

    switch (mReason) {
      case nsINetworkPredictor::PREDICT_LOAD:
        gPredictor->PredictForPageload(mTargetURI, mVerifier, 0, mEnqueueTime);
        break;
      case nsINetworkPredictor::PREDICT_STARTUP:
        gPredictor->PredictForStartup(mVerifier, mEnqueueTime);
        break;
      default:
        MOZ_ASSERT(false, "Got unexpected value for predict reason");
        rv = NS_ERROR_UNEXPECTED;
    }

    gPredictor->FreeSpaceInQueue();

    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_PREDICT_WORK_TIME,
                                   startTime);

    gPredictor->MaybeScheduleCleanup();

    return rv;
  }

private:
  Predictor::UriInfo mTargetURI;
  Predictor::UriInfo mSourceURI;
  PredictorPredictReason mReason;
  PredictorVerifierHandle mVerifier;
  TimeStamp mEnqueueTime;
};

// Predicting for a link is easy, and doesn't require the round-trip to the
// predictor thread and back to the main thread, since we don't have to hit the
// db for that.
void
Predictor::PredictForLink(nsIURI *targetURI, nsIURI *sourceURI,
                          nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread(), "Predicting for link off main thread");

  if (!mSpeculativeService) {
    return;
  }

  if (!mEnableHoverOnSSL) {
    bool isSSL = false;
    sourceURI->SchemeIs("https", &isSSL);
    if (isSSL) {
      // We don't want to predict from an HTTPS page, to avoid info leakage
      PREDICTOR_LOG(("Not predicting for link hover - on an SSL page"));
      return;
    }
  }

  mSpeculativeService->SpeculativeConnect(targetURI, nullptr);
  if (verifier) {
    verifier->OnPredictPreconnect(targetURI);
  }
}

// This runnable runs on the main thread, and is responsible for actually
// firing off predictive actions (such as TCP/TLS preconnects and DNS lookups)
class PredictionRunner : public nsRunnable
{
public:
  PredictionRunner(PredictorVerifierHandle &verifier,
                   TimeStamp predictStartTime)
    :mVerifier(verifier)
    ,mPredictStartTime(predictStartTime)
  { }

  void AddPreconnect(const nsACString &uri)
  {
    mPreconnects.AppendElement(uri);
  }

  void AddPreresolve(const nsACString &uri)
  {
    mPreresolves.AppendElement(uri);
  }

  bool HasWork() const
  {
    return !(mPreconnects.IsEmpty() && mPreresolves.IsEmpty());
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread(), "Running prediction off main thread");

    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_PREDICT_TIME_TO_ACTION,
                                   mPredictStartTime);

    uint32_t len, i;

    len = mPreconnects.Length();
    for (i = 0; i < len; ++i) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), mPreconnects[i]);
      if (NS_FAILED(rv)) {
        continue;
      }
      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PREDICTIONS> totalPredictions;
      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS> totalPreconnects;
      ++totalPredictions;
      ++totalPreconnects;
      gPredictor->mSpeculativeService->SpeculativeConnect(uri, gPredictor);
      if (mVerifier) {
        mVerifier->OnPredictPreconnect(uri);
      }
    }

    len = mPreresolves.Length();
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    for (i = 0; i < len; ++i) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), mPreresolves[i]);
      if (NS_FAILED(rv)) {
        continue;
      }

      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PREDICTIONS> totalPredictions;
      Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRERESOLVES> totalPreresolves;
      ++totalPredictions;
      ++totalPreresolves;
      nsAutoCString hostname;
      uri->GetAsciiHost(hostname);
      nsCOMPtr<nsICancelable> tmpCancelable;
      gPredictor->mDnsService->AsyncResolve(hostname,
                                            (nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
                                             nsIDNSService::RESOLVE_SPECULATE),
                                            gPredictor->mDNSListener, nullptr,
                                            getter_AddRefs(tmpCancelable));
      if (mVerifier) {
        mVerifier->OnPredictDNS(uri);
      }
    }

    mPreconnects.Clear();
    mPreresolves.Clear();

    return NS_OK;
  }

private:
  nsTArray<nsCString> mPreconnects;
  nsTArray<nsCString> mPreresolves;
  PredictorVerifierHandle mVerifier;
  TimeStamp mPredictStartTime;
};

// This calculates how much to degrade our confidence in our data based on
// the last time this top-level resource was loaded. This "global degradation"
// applies to *all* subresources we have associated with the top-level
// resource. This will be in addition to any reduction in confidence we have
// associated with a particular subresource.
int
Predictor::CalculateGlobalDegradation(PRTime now, PRTime lastLoad)
{
  int globalDegradation;
  PRTime delta = now - lastLoad;
  if (delta < ONE_DAY) {
    globalDegradation = mPageDegradationDay;
  } else if (delta < ONE_WEEK) {
    globalDegradation = mPageDegradationWeek;
  } else if (delta < ONE_MONTH) {
    globalDegradation = mPageDegradationMonth;
  } else if (delta < ONE_YEAR) {
    globalDegradation = mPageDegradationYear;
  } else {
    globalDegradation = mPageDegradationMax;
  }

  Telemetry::Accumulate(Telemetry::PREDICTOR_GLOBAL_DEGRADATION,
                        globalDegradation);
  return globalDegradation;
}

// This calculates our overall confidence that a particular subresource will be
// loaded as part of a top-level load.
// @param baseConfidence - the basic confidence we have for this subresource,
//                         which is the percentage of time this top-level load
//                         loads the subresource in question
// @param lastHit - the timestamp of the last time we loaded this subresource as
//                  part of this top-level load
// @param lastPossible - the timestamp of the last time we performed this
//                       top-level load
// @param globalDegradation - the degradation for this top-level load as
//                            determined by CalculateGlobalDegradation
int
Predictor::CalculateConfidence(int baseConfidence, PRTime lastHit,
                               PRTime lastPossible, int globalDegradation)
{
  Telemetry::AutoCounter<Telemetry::PREDICTOR_PREDICTIONS_CALCULATED> predictionsCalculated;
  ++predictionsCalculated;

  int maxConfidence = 100;
  int confidenceDegradation = 0;

  if (lastHit < lastPossible) {
    // We didn't load this subresource the last time this top-level load was
    // performed, so let's not bother preconnecting (at the very least).
    maxConfidence = mPreconnectMinConfidence - 1;

    // Now calculate how much we want to degrade our confidence based on how
    // long it's been between the last time we did this top-level load and the
    // last time this top-level load included this subresource.
    PRTime delta = lastPossible - lastHit;
    if (delta == 0) {
      confidenceDegradation = 0;
    } else if (delta < ONE_DAY) {
      confidenceDegradation = mSubresourceDegradationDay;
    } else if (delta < ONE_WEEK) {
      confidenceDegradation = mSubresourceDegradationWeek;
    } else if (delta < ONE_MONTH) {
      confidenceDegradation = mSubresourceDegradationMonth;
    } else if (delta < ONE_YEAR) {
      confidenceDegradation = mSubresourceDegradationYear;
    } else {
      confidenceDegradation = mSubresourceDegradationMax;
      maxConfidence = 0;
    }
  }

  // Calculate our confidence and clamp it to between 0 and maxConfidence
  // (<= 100)
  int confidence = baseConfidence - confidenceDegradation - globalDegradation;
  confidence = std::max(confidence, 0);
  confidence = std::min(confidence, maxConfidence);

  Telemetry::Accumulate(Telemetry::PREDICTOR_BASE_CONFIDENCE, baseConfidence);
  Telemetry::Accumulate(Telemetry::PREDICTOR_SUBRESOURCE_DEGRADATION,
                        confidenceDegradation);
  Telemetry::Accumulate(Telemetry::PREDICTOR_CONFIDENCE, confidence);
  return confidence;
}

// (Maybe) adds a predictive action to the prediction runner, based on our
// calculated confidence for the subresource in question.
void
Predictor::SetupPrediction(int confidence, const nsACString &uri,
                           PredictionRunner *runner)
{
    if (confidence >= mPreconnectMinConfidence) {
      runner->AddPreconnect(uri);
    } else if (confidence >= mPreresolveMinConfidence) {
      runner->AddPreresolve(uri);
    }
}

// This gets the data about the top-level load from our database, either from
// the pages table (which is specific to a particular URI), or from the hosts
// table (which is for a particular origin).
bool
Predictor::LookupTopLevel(QueryType queryType, const nsACString &key,
                          TopLevelInfo &info)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LookupTopLevel called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id, loads, last_load FROM moz_pages WHERE "
                           "uri = :key;"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id, loads, last_load FROM moz_hosts WHERE "
                           "origin = :key;"));
  }
  NS_ENSURE_TRUE(stmt, false);
  mozStorageStatementScoper scope(stmt);

  nsresult rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("key"), key);
  NS_ENSURE_SUCCESS(rv, false);

  bool hasRows;
  rv = stmt->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, false);

  if (!hasRows) {
    return false;
  }

  rv = stmt->GetInt32(0, &info.id);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->GetInt32(1, &info.loadCount);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->GetInt64(2, &info.lastLoad);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

// Insert data about either a top-level page or a top-level origin into
// the database.
void
Predictor::AddTopLevel(QueryType queryType, const nsACString &key, PRTime now)
{
  MOZ_ASSERT(!NS_IsMainThread(), "AddTopLevel called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_pages (uri, loads, last_load) "
                           "VALUES (:key, 1, :now);"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_hosts (origin, loads, last_load) "
                           "VALUES (:key, 1, :now);"));
  }
  if (!stmt) {
    return;
  }
  mozStorageStatementScoper scope(stmt);

  // Loading a page implicitly makes the predictor learn about the page,
  // so since we don't have it already, let's add it.
  nsresult rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("key"), key);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
  RETURN_IF_FAILED(rv);

  rv = stmt->Execute();
}

// Update data about either a top-level page or a top-level origin in the
// database.
void
Predictor::UpdateTopLevel(QueryType queryType, const TopLevelInfo &info,
                          PRTime now)
{
  MOZ_ASSERT(!NS_IsMainThread(), "UpdateTopLevel called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE moz_pages SET loads = :load_count, "
                           "last_load = :now WHERE id = :id;"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE moz_hosts SET loads = :load_count, "
                           "last_load = :now WHERE id = :id;"));
  }
  if (!stmt) {
    return;
  }
  mozStorageStatementScoper scope(stmt);

  int32_t newLoadCount = info.loadCount + 1;
  if (newLoadCount <= 0) {
    PREDICTOR_LOG(("Predictor::UpdateTopLevel type %d id %d load count "
                   "overflow\n", queryType, info.id));
    newLoadCount = info.loadCount;
    Telemetry::AutoCounter<Telemetry::PREDICTOR_LOAD_COUNT_OVERFLOWS> loadCountOverflows;
    ++loadCountOverflows;
  }

  // First, let's update the page in the database, since loading a page
  // implicitly learns about the page.
  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("load_count"),
                                      newLoadCount);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("id"), info.id);
  RETURN_IF_FAILED(rv);

  rv = stmt->Execute();
}

// Tries to predict for a top-level load (either page-based or origin-based).
// Returns false if it failed to predict at all, true if it did some sort of
// prediction.
// @param queryType - whether to predict based on page or origin
// @param info - the db info about the top-level resource
bool
Predictor::TryPredict(QueryType queryType, const TopLevelInfo &info, PRTime now,
                      PredictorVerifierHandle &verifier,
                      TimeStamp &predictStartTime)
{
  MOZ_ASSERT(!NS_IsMainThread(), "TryPredict called on main thread.");

  if (!info.loadCount) {
    PREDICTOR_LOG(("Predictor::TryPredict info.loadCount is zero!\n"));
    Telemetry::AutoCounter<Telemetry::PREDICTOR_LOAD_COUNT_IS_ZERO> loadCountZeroes;
    ++loadCountZeroes;
    return false;
  }

  int globalDegradation = CalculateGlobalDegradation(now, info.lastLoad);

  // Now let's look up the subresources we know about for this page
  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT uri, hits, last_hit FROM moz_subresources "
                           "WHERE pid = :id;"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT origin, hits, last_hit FROM moz_subhosts "
                           "WHERE hid = :id;"));
  }
  NS_ENSURE_TRUE(stmt, false);
  mozStorageStatementScoper scope(stmt);

  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("id"), info.id);
  NS_ENSURE_SUCCESS(rv, false);

  bool hasRows;
  rv = stmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return false;
  }

  nsRefPtr<PredictionRunner> runner =
    new PredictionRunner(verifier, predictStartTime);

  while (hasRows) {
    int32_t hitCount;
    PRTime lastHit;
    nsAutoCString subresource;
    int baseConfidence, confidence;

    // We use goto nextrow here instead of just failing, because we want
    // to do some sort of prediction if at all possible. Of course, it's
    // probably unlikely that subsequent rows will succeed if one fails, but
    // it's worth a shot.

    rv = stmt->GetUTF8String(0, subresource);
    if NS_FAILED(rv) {
      goto nextrow;
    }

    rv = stmt->GetInt32(1, &hitCount);
    if (NS_FAILED(rv)) {
      goto nextrow;
    }

    rv = stmt->GetInt64(2, &lastHit);
    if (NS_FAILED(rv)) {
      goto nextrow;
    }

    baseConfidence = (hitCount * 100) / info.loadCount;
    confidence = CalculateConfidence(baseConfidence, lastHit, info.lastLoad,
                                     globalDegradation);
    SetupPrediction(confidence, subresource, runner);

nextrow:
    rv = stmt->ExecuteStep(&hasRows);
    NS_ENSURE_SUCCESS(rv, false);
  }

  bool predicted = false;

  if (runner->HasWork()) {
    NS_DispatchToMainThread(runner);
    predicted = true;
  }

  return predicted;
}

// Find out if a top-level page is likely to redirect.
bool
Predictor::WouldRedirect(const TopLevelInfo &info, PRTime now, UriInfo &newUri)
{
  MOZ_ASSERT(!NS_IsMainThread(), "WouldRedirect called on main thread.");

  if (!info.loadCount) {
    PREDICTOR_LOG(("Predictor::WouldRedirect info.loadCount is zero!\n"));
    Telemetry::AutoCounter<Telemetry::PREDICTOR_LOAD_COUNT_IS_ZERO> loadCountZeroes;
    ++loadCountZeroes;
    return false;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT uri, origin, hits, last_hit "
                         "FROM moz_redirects WHERE pid = :id;"));
  NS_ENSURE_TRUE(stmt, false);
  mozStorageStatementScoper scope(stmt);

  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("id"), info.id);
  NS_ENSURE_SUCCESS(rv, false);

  bool hasRows;
  rv = stmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return false;
  }

  rv = stmt->GetUTF8String(0, newUri.spec);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->GetUTF8String(1, newUri.origin);
  NS_ENSURE_SUCCESS(rv, false);

  int32_t hitCount;
  rv = stmt->GetInt32(2, &hitCount);
  NS_ENSURE_SUCCESS(rv, false);

  PRTime lastHit;
  rv = stmt->GetInt64(3, &lastHit);
  NS_ENSURE_SUCCESS(rv, false);

  int globalDegradation = CalculateGlobalDegradation(now, info.lastLoad);
  int baseConfidence = (hitCount * 100) / info.loadCount;
  int confidence = CalculateConfidence(baseConfidence, lastHit, info.lastLoad,
                                       globalDegradation);

  if (confidence > mRedirectLikelyConfidence) {
    return true;
  }

  return false;
}

// This will add a page to our list of startup pages if it's being loaded
// before our startup window has expired.
void
Predictor::MaybeLearnForStartup(const UriInfo &uri, const PRTime now)
{
  MOZ_ASSERT(!NS_IsMainThread(), "MaybeLearnForStartup called on main thread.");

  if ((now - mStartupTime) < STARTUP_WINDOW) {
    LearnForStartup(uri);
  }
}

const int MAX_PAGELOAD_DEPTH = 10;

// This is the driver for prediction based on a new pageload.
void
Predictor::PredictForPageload(const UriInfo &uri,
                              PredictorVerifierHandle &verifier,
                              int stackCount, TimeStamp &predictStartTime)
{
  MOZ_ASSERT(!NS_IsMainThread(), "PredictForPageload called on main thread.");

  if (stackCount > MAX_PAGELOAD_DEPTH) {
    PREDICTOR_LOG(("Too deep into pageload prediction"));
    return;
  }

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  PRTime now = PR_Now();

  MaybeLearnForStartup(uri, now);

  TopLevelInfo pageInfo;
  TopLevelInfo originInfo;
  bool havePage = LookupTopLevel(QUERY_PAGE, uri.spec, pageInfo);
  bool haveOrigin = LookupTopLevel(QUERY_ORIGIN, uri.origin, originInfo);

  if (!havePage) {
    AddTopLevel(QUERY_PAGE, uri.spec, now);
  } else {
    UpdateTopLevel(QUERY_PAGE, pageInfo, now);
  }

  if (!haveOrigin) {
    AddTopLevel(QUERY_ORIGIN, uri.origin, now);
  } else {
    UpdateTopLevel(QUERY_ORIGIN, originInfo, now);
  }

  UriInfo newUri;
  if (havePage && WouldRedirect(pageInfo, now, newUri)) {
    nsRefPtr<PredictionRunner> runner =
      new PredictionRunner(verifier, predictStartTime);
    runner->AddPreconnect(newUri.spec);
    NS_DispatchToMainThread(runner);
    PredictForPageload(newUri, verifier, stackCount + 1, predictStartTime);
    return;
  }

  bool predicted = false;

  // We always try to be as specific as possible in our predictions, so try
  // to predict based on the full URI before we fall back to the origin.
  if (havePage) {
    predicted = TryPredict(QUERY_PAGE, pageInfo, now, verifier,
                           predictStartTime);
  }

  if (!predicted && haveOrigin) {
    predicted = TryPredict(QUERY_ORIGIN, originInfo, now, verifier,
                           predictStartTime);
  }

  if (!predicted) {
    Telemetry::AccumulateTimeDelta(
      Telemetry::PREDICTOR_PREDICT_TIME_TO_INACTION,
      predictStartTime);
  }
}

// This is the driver for predicting at browser startup time based on pages that
// have previously been loaded close to startup.
void
Predictor::PredictForStartup(PredictorVerifierHandle &verifier,
                             TimeStamp &predictStartTime)
{
  MOZ_ASSERT(!NS_IsMainThread(), "PredictForStartup called on main thread");

  if (!mStartupCount) {
    PREDICTOR_LOG(("Predictor::PredictForStartup mStartupCount is zero!\n"));
    Telemetry::AutoCounter<Telemetry::PREDICTOR_STARTUP_COUNT_IS_ZERO> startupCountZeroes;
    ++startupCountZeroes;
    return;
  }

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT uri, hits, last_hit FROM moz_startup_pages;"));
  if (!stmt) {
    return;
  }
  mozStorageStatementScoper scope(stmt);
  nsresult rv;
  bool hasRows;

  nsRefPtr<PredictionRunner> runner =
    new PredictionRunner(verifier, predictStartTime);

  rv = stmt->ExecuteStep(&hasRows);
  RETURN_IF_FAILED(rv);

  while (hasRows) {
    nsAutoCString uri;
    int32_t hitCount;
    PRTime lastHit;
    int baseConfidence, confidence;

    // We use goto nextrow here instead of just failling, because we want
    // to do some sort of prediction if at all possible. Of course, it's
    // probably unlikely that subsequent rows will succeed if one fails, but
    // it's worth a shot.

    rv = stmt->GetUTF8String(0, uri);
    if (NS_FAILED(rv)) {
      goto nextrow;
    }

    rv = stmt->GetInt32(1, &hitCount);
    if (NS_FAILED(rv)) {
      goto nextrow;
    }

    rv = stmt->GetInt64(2, &lastHit);
    if (NS_FAILED(rv)) {
      goto nextrow;
    }

    baseConfidence = (hitCount * 100) / mStartupCount;
    confidence = CalculateConfidence(baseConfidence, lastHit,
                                     mLastStartupTime, 0);
    SetupPrediction(confidence, uri, runner);

nextrow:
    rv = stmt->ExecuteStep(&hasRows);
    RETURN_IF_FAILED(rv);
  }

  if (runner->HasWork()) {
    NS_DispatchToMainThread(runner);
  } else {
    Telemetry::AccumulateTimeDelta(
      Telemetry::PREDICTOR_PREDICT_TIME_TO_INACTION,
      predictStartTime);
  }
}

// All URIs we get passed *must* be http or https if they're not null. This
// helps ensure that.
static bool
IsNullOrHttp(nsIURI *uri)
{
  if (!uri) {
    return true;
  }

  bool isHTTP = false;
  uri->SchemeIs("http", &isHTTP);
  if (!isHTTP) {
    uri->SchemeIs("https", &isHTTP);
  }

  return isHTTP;
}

nsresult
Predictor::ReserveSpaceInQueue()
{
  MutexAutoLock lock(mQueueSizeLock);

  if (mQueueSize >= mMaxQueueSize) {
    PREDICTOR_LOG(("Not enqueuing event - queue too large"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  mQueueSize++;
  return NS_OK;
}

void
Predictor::FreeSpaceInQueue()
{
  MutexAutoLock lock(mQueueSizeLock);
  MOZ_ASSERT(mQueueSize > 0, "unexpected mQueueSize");
  mQueueSize--;
}

// Called from the main thread to initiate predictive actions
NS_IMETHODIMP
Predictor::Predict(nsIURI *targetURI, nsIURI *sourceURI,
                   PredictorPredictReason reason,
                   nsILoadContext *loadContext,
                   nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  if (!mInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (loadContext && loadContext->UsePrivateBrowsing()) {
    // Don't want to do anything in PB mode
    return NS_OK;
  }

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    // Nothing we can do for non-HTTP[S] schemes
    return NS_OK;
  }

  // Ensure we've been given the appropriate arguments for the kind of
  // prediction we're being asked to do
  switch (reason) {
    case nsINetworkPredictor::PREDICT_LINK:
      if (!targetURI || !sourceURI) {
        return NS_ERROR_INVALID_ARG;
      }
      // Link hover is a special case where we can predict without hitting the
      // db, so let's go ahead and fire off that prediction here.
      PredictForLink(targetURI, sourceURI, verifier);
      return NS_OK;
    case nsINetworkPredictor::PREDICT_LOAD:
      if (!targetURI || sourceURI) {
        return NS_ERROR_INVALID_ARG;
      }
      break;
    case nsINetworkPredictor::PREDICT_STARTUP:
      if (targetURI || sourceURI) {
        return NS_ERROR_INVALID_ARG;
      }
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  Telemetry::AutoCounter<Telemetry::PREDICTOR_PREDICT_ATTEMPTS> predictAttempts;
  ++predictAttempts;

  nsresult rv = ReserveSpaceInQueue();
  if (NS_FAILED(rv)) {
    Telemetry::AutoCounter<Telemetry::PREDICTOR_PREDICT_FULL_QUEUE> predictFullQueue;
    ++predictFullQueue;
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<PredictionEvent> event = new PredictionEvent(targetURI,
                                                        sourceURI,
                                                        reason,
                                                        verifier);
  return mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

// A runnable for updating our information in the database. This must always
// be dispatched to the predictor thread.
class LearnEvent : public nsRunnable
{
public:
  LearnEvent(nsIURI *targetURI, nsIURI *sourceURI, PredictorLearnReason reason)
    :mReason(reason)
  {
    MOZ_ASSERT(NS_IsMainThread(), "Creating learn event off main thread");

    mEnqueueTime = TimeStamp::Now();

    targetURI->GetAsciiSpec(mTargetURI.spec);
    ExtractOrigin(targetURI, mTargetURI.origin);
    if (sourceURI) {
      sourceURI->GetAsciiSpec(mSourceURI.spec);
      ExtractOrigin(sourceURI, mSourceURI.origin);
    }
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Running learn off main thread");

    nsresult rv = NS_OK;

    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_LEARN_WAIT_TIME,
                                   mEnqueueTime);

    TimeStamp startTime = TimeStamp::Now();

    switch (mReason) {
    case nsINetworkPredictor::LEARN_LOAD_TOPLEVEL:
      gPredictor->LearnForToplevel(mTargetURI);
      break;
    case nsINetworkPredictor::LEARN_LOAD_REDIRECT:
      gPredictor->LearnForRedirect(mTargetURI, mSourceURI);
      break;
    case nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE:
      gPredictor->LearnForSubresource(mTargetURI, mSourceURI);
      break;
    case nsINetworkPredictor::LEARN_STARTUP:
      gPredictor->LearnForStartup(mTargetURI);
      break;
    default:
      MOZ_ASSERT(false, "Got unexpected value for learn reason");
      rv = NS_ERROR_UNEXPECTED;
    }

    gPredictor->FreeSpaceInQueue();

    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_LEARN_WORK_TIME,
                                   startTime);

    gPredictor->MaybeScheduleCleanup();

    return rv;
  }
private:
  Predictor::UriInfo mTargetURI;
  Predictor::UriInfo mSourceURI;
  PredictorLearnReason mReason;
  TimeStamp mEnqueueTime;
};

void
Predictor::LearnForToplevel(const UriInfo &uri)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LearnForToplevel called on main thread.");

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  PRTime now = PR_Now();

  MaybeLearnForStartup(uri, now);

  TopLevelInfo pageInfo;
  TopLevelInfo originInfo;
  bool havePage = LookupTopLevel(QUERY_PAGE, uri.spec, pageInfo);
  bool haveOrigin = LookupTopLevel(QUERY_ORIGIN, uri.origin, originInfo);

  if (!havePage) {
    AddTopLevel(QUERY_PAGE, uri.spec, now);
  } else {
    UpdateTopLevel(QUERY_PAGE, pageInfo, now);
  }

  if (!haveOrigin) {
    AddTopLevel(QUERY_ORIGIN, uri.origin, now);
  } else {
    UpdateTopLevel(QUERY_ORIGIN, originInfo, now);
  }
}

// Queries to look up information about a *specific* subresource associated
// with a *specific* top-level load.
bool
Predictor::LookupSubresource(QueryType queryType, const int32_t parentId,
                             const nsACString &key, SubresourceInfo &info)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LookupSubresource called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id, hits, last_hit FROM moz_subresources "
                           "WHERE pid = :parent_id AND uri = :key;"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("SELECT id, hits, last_hit FROM moz_subhosts WHERE "
                           "hid = :parent_id AND origin = :key;"));
  }
  NS_ENSURE_TRUE(stmt, false);
  mozStorageStatementScoper scope(stmt);

  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("parent_id"),
                                      parentId);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("key"), key);
  NS_ENSURE_SUCCESS(rv, false);

  bool hasRows;
  rv = stmt->ExecuteStep(&hasRows);
  NS_ENSURE_SUCCESS(rv, false);
  if (!hasRows) {
    return false;
  }

  rv = stmt->GetInt32(0, &info.id);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->GetInt32(1, &info.hitCount);
  NS_ENSURE_SUCCESS(rv, false);

  rv = stmt->GetInt64(2, &info.lastHit);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

// Add information about a new subresource associated with a top-level load.
void
Predictor::AddSubresource(QueryType queryType, const int32_t parentId,
                          const nsACString &key, const PRTime now)
{
  MOZ_ASSERT(!NS_IsMainThread(), "AddSubresource called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_subresources "
                           "(pid, uri, hits, last_hit) VALUES "
                           "(:parent_id, :key, 1, :now);"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_subhosts "
                           "(hid, origin, hits, last_hit) VALUES "
                           "(:parent_id, :key, 1, :now);"));
  }
  if (!stmt) {
    return;
  }
  mozStorageStatementScoper scope(stmt);

  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("parent_id"),
                                      parentId);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("key"), key);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
  RETURN_IF_FAILED(rv);

  rv = stmt->Execute();
}

// Update the information about a particular subresource associated with a
// top-level load
void
Predictor::UpdateSubresource(QueryType queryType, const SubresourceInfo &info,
                             const PRTime now, const int32_t parentCount)
{
  MOZ_ASSERT(!NS_IsMainThread(), "UpdateSubresource called on main thread.");

  nsCOMPtr<mozIStorageStatement> stmt;
  if (queryType == QUERY_PAGE) {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE moz_subresources SET hits = :hit_count, "
                           "last_hit = :now WHERE id = :id;"));
  } else {
    stmt = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE moz_subhosts SET hits = :hit_count, "
                           "last_hit = :now WHERE id = :id;"));
  }
  if (!stmt) {
    return;
  }
  mozStorageStatementScoper scope(stmt);

  // Make sure the hit count can't be more that its parent
  int32_t hitCount = std::min((info.hitCount + 1), parentCount);

  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hit_count"),
                                      hitCount);

  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
  RETURN_IF_FAILED(rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("id"), info.id);
  RETURN_IF_FAILED(rv);

  rv = stmt->Execute();
}

// Called when a subresource has been hit from a top-level load. Uses the two
// helper functions above to update the database appropriately.
void
Predictor::LearnForSubresource(const UriInfo &targetURI,
                               const UriInfo &sourceURI)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LearnForSubresource called on main thread.");

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  TopLevelInfo pageInfo, originInfo;
  bool havePage = LookupTopLevel(QUERY_PAGE, sourceURI.spec, pageInfo);
  bool haveOrigin = LookupTopLevel(QUERY_ORIGIN, sourceURI.origin,
                                   originInfo);

  if (!havePage && !haveOrigin) {
    // Nothing to do, since we know nothing about the top level resource
    return;
  }

  SubresourceInfo resourceInfo;
  bool haveResource = false;
  if (havePage) {
    haveResource = LookupSubresource(QUERY_PAGE, pageInfo.id, targetURI.spec,
                                     resourceInfo);
  }

  SubresourceInfo hostInfo;
  bool haveHost = false;
  if (haveOrigin) {
    haveHost = LookupSubresource(QUERY_ORIGIN, originInfo.id, targetURI.origin,
                                 hostInfo);
  }

  if (haveResource) {
    UpdateSubresource(QUERY_PAGE, resourceInfo, pageInfo.lastLoad, pageInfo.loadCount);
  } else if (havePage) {
    AddSubresource(QUERY_PAGE, pageInfo.id, targetURI.spec, pageInfo.lastLoad);
  }
  // Can't add a subresource to a page we don't have in our db.

  if (haveHost) {
    UpdateSubresource(QUERY_ORIGIN, hostInfo, originInfo.lastLoad, originInfo.loadCount);
  } else if (haveOrigin) {
    AddSubresource(QUERY_ORIGIN, originInfo.id, targetURI.origin, originInfo.lastLoad);
  }
  // Can't add a subhost to a host we don't have in our db
}

// This is called when a top-level loaded ended up redirecting to a different
// URI so we can keep track of that fact.
void
Predictor::LearnForRedirect(const UriInfo &targetURI, const UriInfo &sourceURI)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LearnForRedirect called on main thread.");

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  PRTime now = PR_Now();
  nsresult rv;

  nsCOMPtr<mozIStorageStatement> getPage = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT id FROM moz_pages WHERE uri = :spec;"));
  if (!getPage) {
    return;
  }
  mozStorageStatementScoper scopedPage(getPage);

  // look up source in moz_pages
  rv = getPage->BindUTF8StringByName(NS_LITERAL_CSTRING("spec"),
                                     sourceURI.spec);
  RETURN_IF_FAILED(rv);

  bool hasRows;
  rv = getPage->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return;
  }

  int32_t pageId;
  rv = getPage->GetInt32(0, &pageId);
  RETURN_IF_FAILED(rv);

  nsCOMPtr<mozIStorageStatement> getRedirect = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT id, hits FROM moz_redirects WHERE "
                         "pid = :page_id AND uri = :spec;"));
  if (!getRedirect) {
    return;
  }
  mozStorageStatementScoper scopedRedirect(getRedirect);

  rv = getRedirect->BindInt32ByName(NS_LITERAL_CSTRING("page_id"), pageId);
  RETURN_IF_FAILED(rv);

  rv = getRedirect->BindUTF8StringByName(NS_LITERAL_CSTRING("spec"),
                                         targetURI.spec);
  RETURN_IF_FAILED(rv);

  rv = getRedirect->ExecuteStep(&hasRows);
  RETURN_IF_FAILED(rv);

  if (!hasRows) {
    // This is the first time we've seen this top-level redirect to this URI
    nsCOMPtr<mozIStorageStatement> addRedirect = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_redirects "
                           "(pid, uri, origin, hits, last_hit) VALUES "
                           "(:page_id, :spec, :origin, 1, :now);"));
    if (!addRedirect) {
      return;
    }
    mozStorageStatementScoper scopedAdd(addRedirect);

    rv = addRedirect->BindInt32ByName(NS_LITERAL_CSTRING("page_id"), pageId);
    RETURN_IF_FAILED(rv);

    rv = addRedirect->BindUTF8StringByName(NS_LITERAL_CSTRING("spec"),
                                           targetURI.spec);
    RETURN_IF_FAILED(rv);

    rv = addRedirect->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"),
                                           targetURI.origin);
    RETURN_IF_FAILED(rv);

    rv = addRedirect->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
    RETURN_IF_FAILED(rv);

    rv = addRedirect->Execute();
  } else {
    // We've seen this redirect before
    int32_t redirId, hits;
    rv = getRedirect->GetInt32(0, &redirId);
    RETURN_IF_FAILED(rv);

    rv = getRedirect->GetInt32(1, &hits);
    RETURN_IF_FAILED(rv);

    nsCOMPtr<mozIStorageStatement> updateRedirect =
      mStatements.GetCachedStatement(
          NS_LITERAL_CSTRING("UPDATE moz_redirects SET hits = :hits, "
                             "last_hit = :now WHERE id = :redir;"));
    if (!updateRedirect) {
      return;
    }
    mozStorageStatementScoper scopedUpdate(updateRedirect);

    rv = updateRedirect->BindInt32ByName(NS_LITERAL_CSTRING("hits"), hits + 1);
    RETURN_IF_FAILED(rv);

    rv = updateRedirect->BindInt64ByName(NS_LITERAL_CSTRING("now"), now);
    RETURN_IF_FAILED(rv);

    rv = updateRedirect->BindInt32ByName(NS_LITERAL_CSTRING("redir"), redirId);
    RETURN_IF_FAILED(rv);

    updateRedirect->Execute();
  }
}

// Add information about a top-level load to our list of startup pages
void
Predictor::LearnForStartup(const UriInfo &uri)
{
  MOZ_ASSERT(!NS_IsMainThread(), "LearnForStartup called on main thread.");

  if (NS_FAILED(EnsureInitStorage())) {
    return;
  }

  nsCOMPtr<mozIStorageStatement> getPage = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT id, hits FROM moz_startup_pages WHERE "
                         "uri = :origin;"));
  if (!getPage) {
    return;
  }
  mozStorageStatementScoper scopedPage(getPage);
  nsresult rv;

  rv = getPage->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), uri.origin);
  RETURN_IF_FAILED(rv);

  bool hasRows;
  rv = getPage->ExecuteStep(&hasRows);
  RETURN_IF_FAILED(rv);

  if (hasRows) {
    // We've loaded this page on startup before
    int32_t pageId, hitCount;

    rv = getPage->GetInt32(0, &pageId);
    RETURN_IF_FAILED(rv);

    rv = getPage->GetInt32(1, &hitCount);
    RETURN_IF_FAILED(rv);

    nsCOMPtr<mozIStorageStatement> updatePage = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("UPDATE moz_startup_pages SET hits = :hit_count, "
                           "last_hit = :startup_time WHERE id = :page_id;"));
    if (!updatePage) {
      return;
    }
    mozStorageStatementScoper scopedUpdate(updatePage);

    rv = updatePage->BindInt32ByName(NS_LITERAL_CSTRING("hit_count"),
                                     hitCount + 1);
    RETURN_IF_FAILED(rv);

    rv = updatePage->BindInt64ByName(NS_LITERAL_CSTRING("startup_time"),
                                     mStartupTime);
    RETURN_IF_FAILED(rv);

    rv = updatePage->BindInt32ByName(NS_LITERAL_CSTRING("page_id"), pageId);
    RETURN_IF_FAILED(rv);

    updatePage->Execute();
  } else {
    // New startup page
    nsCOMPtr<mozIStorageStatement> addPage = mStatements.GetCachedStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_startup_pages (uri, hits, "
                           "last_hit) VALUES (:origin, 1, :startup_time);"));
    if (!addPage) {
      return;
    }
    mozStorageStatementScoper scopedAdd(addPage);
    rv = addPage->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"),
                                       uri.origin);
    RETURN_IF_FAILED(rv);

    rv = addPage->BindInt64ByName(NS_LITERAL_CSTRING("startup_time"),
                                  mStartupTime);
    RETURN_IF_FAILED(rv);

    addPage->Execute();
  }
}

// Called from the main thread to update the database
NS_IMETHODIMP
Predictor::Learn(nsIURI *targetURI, nsIURI *sourceURI,
                 PredictorLearnReason reason,
                 nsILoadContext *loadContext)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  if (!mInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (loadContext && loadContext->UsePrivateBrowsing()) {
    // Don't want to do anything in PB mode
    return NS_OK;
  }

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_ERROR_INVALID_ARG;
  }

  switch (reason) {
  case nsINetworkPredictor::LEARN_LOAD_TOPLEVEL:
  case nsINetworkPredictor::LEARN_STARTUP:
    if (!targetURI || sourceURI) {
      return NS_ERROR_INVALID_ARG;
    }
    break;
  case nsINetworkPredictor::LEARN_LOAD_REDIRECT:
  case nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE:
    if (!targetURI || !sourceURI) {
      return NS_ERROR_INVALID_ARG;
    }
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  Telemetry::AutoCounter<Telemetry::PREDICTOR_LEARN_ATTEMPTS> learnAttempts;
  ++learnAttempts;

  nsresult rv = ReserveSpaceInQueue();
  if (NS_FAILED(rv)) {
    Telemetry::AutoCounter<Telemetry::PREDICTOR_LEARN_FULL_QUEUE> learnFullQueue;
    ++learnFullQueue;
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<LearnEvent> event = new LearnEvent(targetURI, sourceURI, reason);
  return mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

// Runnable to clear out the database. Dispatched from the main thread to the
// predictor thread
class PredictorResetEvent : public nsRunnable
{
public:
  PredictorResetEvent()
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Running reset on main thread");

    gPredictor->ResetInternal();

    return NS_OK;
  }
};

// Helper that actually does the database wipe.
void
Predictor::ResetInternal()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Resetting db on main thread");

  nsresult rv = EnsureInitStorage();
  RETURN_IF_FAILED(rv);

  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_redirects;"));
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_startup_pages;"));
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_startups;"));

  // These cascade to moz_subresources and moz_subhosts, respectively.
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_pages;"));
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_hosts;"));

  VacuumDatabase();

  // Go ahead and ensure this is flushed to disk
  CommitTransaction();
  BeginTransaction();
}

// Called on the main thread to clear out all our knowledge. Tabula Rasa FTW!
NS_IMETHODIMP
Predictor::Reset()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  if (!mInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mEnabled) {
    return NS_OK;
  }

  nsRefPtr<PredictorResetEvent> event = new PredictorResetEvent();
  return mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
}

class PredictorCleanupEvent : public nsRunnable
{
public:
  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    gPredictor->Cleanup();
    gPredictor->mCleanupScheduled = false;
    return NS_OK;
  }
};

// Returns the current size (in bytes) of the db file on disk
int64_t
Predictor::GetDBFileSize()
{
  MOZ_ASSERT(!NS_IsMainThread(), "GetDBFileSize called on main thread!");

  nsresult rv = EnsureInitStorage();
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(("GetDBFileSize called without db available!"));
    return 0;
  }

  CommitTransaction();

  nsCOMPtr<mozIStorageStatement> countStmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA page_count;"));
  if (!countStmt) {
    return 0;
  }
  mozStorageStatementScoper scopedCount(countStmt);
  bool hasRows;
  rv = countStmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }
  int64_t pageCount;
  rv = countStmt->GetInt64(0, &pageCount);
  if (NS_FAILED(rv)) {
    return 0;
  }

  nsCOMPtr<mozIStorageStatement> sizeStmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA page_size;"));
  if (!sizeStmt) {
    return 0;
  }
  mozStorageStatementScoper scopedSize(sizeStmt);
  rv = sizeStmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }
  int64_t pageSize;
  rv = sizeStmt->GetInt64(0, &pageSize);
  if (NS_FAILED(rv)) {
    return 0;
  }

  BeginTransaction();

  return pageCount * pageSize;
}

// Returns the size (in bytes) that the db file will consume on disk AFTER we
// vacuum the db.
int64_t
Predictor::GetDBFileSizeAfterVacuum()
{
  MOZ_ASSERT(!NS_IsMainThread(),
             "GetDBFileSizeAfterVacuum called on main thread!");

  CommitTransaction();

  nsCOMPtr<mozIStorageStatement> countStmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA page_count;"));
  if (!countStmt) {
    return 0;
  }
  mozStorageStatementScoper scopedCount(countStmt);
  bool hasRows;
  nsresult rv = countStmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }
  int64_t pageCount;
  rv = countStmt->GetInt64(0, &pageCount);
  if (NS_FAILED(rv)) {
    return 0;
  }

  nsCOMPtr<mozIStorageStatement> sizeStmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA page_size;"));
  if (!sizeStmt) {
    return 0;
  }
  mozStorageStatementScoper scopedSize(sizeStmt);
  rv = sizeStmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }
  int64_t pageSize;
  rv = sizeStmt->GetInt64(0, &pageSize);
  if (NS_FAILED(rv)) {
    return 0;
  }

  nsCOMPtr<mozIStorageStatement> freeStmt = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("PRAGMA freelist_count;"));
  if (!freeStmt) {
    return 0;
  }
  mozStorageStatementScoper scopedFree(freeStmt);
  rv = freeStmt->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }
  int64_t freelistCount;
  rv = freeStmt->GetInt64(0, &freelistCount);
  if (NS_FAILED(rv)) {
    return 0;
  }

  BeginTransaction();

  return (pageCount - freelistCount) * pageSize;
}

void
Predictor::MaybeScheduleCleanup()
{
  MOZ_ASSERT(!NS_IsMainThread(), "MaybeScheduleCleanup called on main thread!");

  // This is a little racy, but it's a nice little shutdown optimization if the
  // race works out the right way.
  if (!mInitialized) {
    return;
  }

  if (mCleanupScheduled) {
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SCHEDULED, false);
    return;
  }

  int64_t dbFileSize = GetDBFileSize();
  if (dbFileSize < mMaxDBSize) {
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SCHEDULED, false);
    return;
  }

  mCleanupScheduled = true;

  nsRefPtr<PredictorCleanupEvent> event = new PredictorCleanupEvent();
  nsresult rv = mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    mCleanupScheduled = false;
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SCHEDULED, false);
  } else {
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SCHEDULED, true);
  }
}

#ifndef ANDROID
static const long long CLEANUP_CUTOFF = ONE_MONTH;
#else
static const long long CLEANUP_CUTOFF = ONE_WEEK;
#endif

void
Predictor::CleanupOrigins(PRTime now)
{
  PRTime cutoff = now - CLEANUP_CUTOFF;

  nsCOMPtr<mozIStorageStatement> deleteOrigins = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_hosts WHERE last_load <= :cutoff"));
  if (!deleteOrigins) {
    return;
  }
  mozStorageStatementScoper scopedOrigins(deleteOrigins);

  nsresult rv = deleteOrigins->BindInt32ByName(NS_LITERAL_CSTRING("cutoff"),
                                               cutoff);
  RETURN_IF_FAILED(rv);

  deleteOrigins->Execute();
}

void
Predictor::CleanupStartupPages(PRTime now)
{
  PRTime cutoff = now - ONE_WEEK;

  nsCOMPtr<mozIStorageStatement> deletePages = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_startup_pages WHERE "
                         "last_hit <= :cutoff"));
  if (!deletePages) {
    return;
  }
  mozStorageStatementScoper scopedPages(deletePages);

  nsresult rv = deletePages->BindInt32ByName(NS_LITERAL_CSTRING("cutoff"),
                                             cutoff);
  RETURN_IF_FAILED(rv);

  deletePages->Execute();
}

int32_t
Predictor::GetSubresourceCount()
{
  nsCOMPtr<mozIStorageStatement> count = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT COUNT(id) FROM moz_subresources"));
  if (!count) {
    return 0;
  }
  mozStorageStatementScoper scopedCount(count);

  bool hasRows;
  nsresult rv = count->ExecuteStep(&hasRows);
  if (NS_FAILED(rv) || !hasRows) {
    return 0;
  }

  int32_t subresourceCount = 0;
  count->GetInt32(0, &subresourceCount);

  return subresourceCount;
}

void
Predictor::Cleanup()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Predictor::Cleanup called on main thread!");

  nsresult rv = EnsureInitStorage();
  if (NS_FAILED(rv)) {
    return;
  }

  int64_t dbFileSize = GetDBFileSize();
  float preservePercentage = static_cast<float>(mPreservePercentage) / 100.0;
  int64_t evictionCutoff =
    static_cast<int64_t>(mMaxDBSize) * preservePercentage;
  if (dbFileSize < evictionCutoff) {
    return;
  }

  CommitTransaction();
  BeginTransaction();

  PRTime now = PR_Now();
  if (mLastCleanupTime) {
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_DELTA,
                          (now - mLastCleanupTime) / 1000);
  }
  mLastCleanupTime = now;

  CleanupOrigins(now);
  CleanupStartupPages(now);

  dbFileSize = GetDBFileSizeAfterVacuum();
  if (dbFileSize < evictionCutoff) {
    // We've deleted enough stuff, time to free up the disk space and be on
    // our way.
    VacuumDatabase();
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SUCCEEDED, true);
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_TIME,
                          (PR_Now() - mLastCleanupTime) / 1000);
    return;
  }

  bool canDelete = true;
  while (canDelete && (dbFileSize >= evictionCutoff)) {
    int32_t subresourceCount = GetSubresourceCount();
    if (!subresourceCount) {
      canDelete = false;
      break;
    }

    // DB size scales pretty much linearly with the number of rows in
    // moz_subresources, so we can guess how many rows we need to delete pretty
    // accurately.
    float percentNeeded = static_cast<float>(dbFileSize - evictionCutoff) /
      static_cast<float>(dbFileSize);

    int32_t subresourcesToDelete = static_cast<int32_t>(percentNeeded * subresourceCount);
    if (!subresourcesToDelete) {
      // We're getting pretty close to nothing here, anyway, so we may as well
      // just trash it all. This delete cascades to moz_subresources, as well.
      rv = mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_pages;"));
      if (NS_FAILED(rv)) {
        canDelete = false;
        break;
      }
    } else {
      nsCOMPtr<mozIStorageStatement> deleteStatement = mStatements.GetCachedStatement(
          NS_LITERAL_CSTRING("DELETE FROM moz_subresources WHERE id IN "
                            "(SELECT id FROM moz_subresources ORDER BY "
                            "last_hit ASC LIMIT :limit);"));
      if (!deleteStatement) {
        canDelete = false;
        break;
      }
      mozStorageStatementScoper scopedDelete(deleteStatement);

      rv = deleteStatement->BindInt32ByName(NS_LITERAL_CSTRING("limit"),
                                            subresourcesToDelete);
      if (NS_FAILED(rv)) {
        canDelete = false;
        break;
      }

      rv = deleteStatement->Execute();
      if (NS_FAILED(rv)) {
        canDelete = false;
        break;
      }

      // Now we clean up pages that no longer reference any subresources
      rv = mDB->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("DELETE FROM moz_pages WHERE id NOT IN "
                             "(SELECT DISTINCT(pid) FROM moz_subresources);"));
      if (NS_FAILED(rv)) {
        canDelete = false;
        break;
      }
    }

    if (canDelete) {
      dbFileSize = GetDBFileSizeAfterVacuum();
    }
  }

  if (!canDelete || (dbFileSize >= evictionCutoff)) {
    // Last-ditch effort to free up space
    ResetInternal();
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SUCCEEDED, false);
  } else {
    // We do this to actually free up the space on disk
    VacuumDatabase();
    Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_SUCCEEDED, true);
  }
  Telemetry::Accumulate(Telemetry::PREDICTOR_CLEANUP_TIME,
                        (PR_Now() - mLastCleanupTime) / 1000);
}

void
Predictor::VacuumDatabase()
{
  MOZ_ASSERT(!NS_IsMainThread(), "VacuumDatabase called on main thread!");

  CommitTransaction();
  mDB->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM;"));
  BeginTransaction();
}

#ifdef PREDICTOR_TESTS
class PredictorPrepareForDnsTestEvent : public nsRunnable
{
public:
  PredictorPrepareForDnsTestEvent(int64_t timestamp, const char *uri)
    :mTimestamp(timestamp)
    ,mUri(uri)
  { }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Preparing for DNS Test on main thread!");
    gPredictor->PrepareForDnsTestInternal(mTimestamp, mUri);
    return NS_OK;
  }

private:
  int64_t mTimestamp;
  nsAutoCString mUri;
};

void
Predictor::PrepareForDnsTestInternal(int64_t timestamp, const nsACString &uri)
{
  nsCOMPtr<mozIStorageStatement> update = mStatements.GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE moz_subresources SET last_hit = :timestamp, "
                          "hits = 2 WHERE uri = :uri;"));
  if (!update) {
    return;
  }
  mozStorageStatementScoper scopedUpdate(update);

  nsresult rv = update->BindInt64ByName(NS_LITERAL_CSTRING("timestamp"),
                                        timestamp);
  RETURN_IF_FAILED(rv);

  rv = update->BindUTF8StringByName(NS_LITERAL_CSTRING("uri"), uri);
  RETURN_IF_FAILED(rv);

  update->Execute();
}
#endif

NS_IMETHODIMP
Predictor::PrepareForDnsTest(int64_t timestamp, const char *uri)
{
#ifdef PREDICTOR_TESTS
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  if (!mInitialized) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<PredictorPrepareForDnsTestEvent> event =
    new PredictorPrepareForDnsTestEvent(timestamp, uri);
  return mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

// Helper functions to make using the predictor easier from native code

static nsresult
EnsureGlobalPredictor(nsINetworkPredictor **aPredictor)
{
  nsresult rv;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1",
                  &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aPredictor = predictor);
  return NS_OK;
}

nsresult
PredictorPredict(nsIURI *targetURI, nsIURI *sourceURI,
                 PredictorPredictReason reason, nsILoadContext *loadContext,
                 nsINetworkPredictorVerifier *verifier)
{
  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  return predictor->Predict(targetURI, sourceURI, reason,
                            loadContext, verifier);
}

nsresult
PredictorLearn(nsIURI *targetURI, nsIURI *sourceURI,
               PredictorLearnReason reason,
               nsILoadContext *loadContext)
{
  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  return predictor->Learn(targetURI, sourceURI, reason, loadContext);
}

nsresult
PredictorLearn(nsIURI *targetURI, nsIURI *sourceURI,
               PredictorLearnReason reason,
               nsILoadGroup *loadGroup)
{
  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadContext> loadContext;

  if (loadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      loadContext = do_GetInterface(callbacks);
    }
  }

  return predictor->Learn(targetURI, sourceURI, reason, loadContext);
}

nsresult
PredictorLearn(nsIURI *targetURI, nsIURI *sourceURI,
               PredictorLearnReason reason,
               nsIDocument *document)
{
  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadContext> loadContext;

  if (document) {
    loadContext = document->GetLoadContext();
  }

  return predictor->Learn(targetURI, sourceURI, reason, loadContext);
}

nsresult
PredictorLearnRedirect(nsIURI *targetURI, nsIChannel *channel,
                       nsILoadContext *loadContext)
{
  nsCOMPtr<nsIURI> sourceURI;
  nsresult rv = channel->GetOriginalURI(getter_AddRefs(sourceURI));
  NS_ENSURE_SUCCESS(rv, rv);

  bool sameUri;
  rv = targetURI->Equals(sourceURI, &sameUri);
  NS_ENSURE_SUCCESS(rv, rv);

  if (sameUri) {
    return NS_OK;
  }

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  return predictor->Learn(targetURI, sourceURI,
                          nsINetworkPredictor::LEARN_LOAD_REDIRECT,
                          loadContext);
}

} // ::mozilla::net
} // ::mozilla
