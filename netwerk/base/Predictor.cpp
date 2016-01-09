/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "Predictor.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsICacheStorage.h"
#include "nsICacheStorageService.h"
#include "nsICancelable.h"
#include "nsIChannel.h"
#include "nsContentUtils.h"
#include "nsIDNSService.h"
#include "nsIDocument.h"
#include "nsIFile.h"
#include "nsIIOService.h"
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
#include "nsThreadUtils.h"
#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif
#include "mozilla/Logging.h"

#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"

#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"

#include "LoadContextInfo.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/net/NeckoChild.h"

#if defined(ANDROID) && !defined(MOZ_WIDGET_GONK)
#include "nsIPropertyBag2.h"
static const int32_t ANDROID_23_VERSION = 10;
#endif

using namespace mozilla;

namespace mozilla {
namespace net {

Predictor *Predictor::sSelf = nullptr;

static LazyLogModule gPredictorLog("NetworkPredictor");

#define PREDICTOR_LOG(args) MOZ_LOG(gPredictorLog, mozilla::LogLevel::Debug, args)

#define RETURN_IF_FAILED(_rv) \
  do { \
    if (NS_FAILED(_rv)) { \
      return; \
    } \
  } while (0)

#define NOW_IN_SECONDS() static_cast<uint32_t>(PR_Now() / PR_USEC_PER_SEC)


const char PREDICTOR_ENABLED_PREF[] = "network.predictor.enabled";
const char PREDICTOR_SSL_HOVER_PREF[] = "network.predictor.enable-hover-on-ssl";

const char PREDICTOR_PAGE_DELTA_DAY_PREF[] =
  "network.predictor.page-degradation.day";
const int32_t PREDICTOR_PAGE_DELTA_DAY_DEFAULT = 0;
const char PREDICTOR_PAGE_DELTA_WEEK_PREF[] =
  "network.predictor.page-degradation.week";
const int32_t PREDICTOR_PAGE_DELTA_WEEK_DEFAULT = 5;
const char PREDICTOR_PAGE_DELTA_MONTH_PREF[] =
  "network.predictor.page-degradation.month";
const int32_t PREDICTOR_PAGE_DELTA_MONTH_DEFAULT = 10;
const char PREDICTOR_PAGE_DELTA_YEAR_PREF[] =
  "network.predictor.page-degradation.year";
const int32_t PREDICTOR_PAGE_DELTA_YEAR_DEFAULT = 25;
const char PREDICTOR_PAGE_DELTA_MAX_PREF[] =
  "network.predictor.page-degradation.max";
const int32_t PREDICTOR_PAGE_DELTA_MAX_DEFAULT = 50;
const char PREDICTOR_SUB_DELTA_DAY_PREF[] =
  "network.predictor.subresource-degradation.day";
const int32_t PREDICTOR_SUB_DELTA_DAY_DEFAULT = 1;
const char PREDICTOR_SUB_DELTA_WEEK_PREF[] =
  "network.predictor.subresource-degradation.week";
const int32_t PREDICTOR_SUB_DELTA_WEEK_DEFAULT = 10;
const char PREDICTOR_SUB_DELTA_MONTH_PREF[] =
  "network.predictor.subresource-degradation.month";
const int32_t PREDICTOR_SUB_DELTA_MONTH_DEFAULT = 25;
const char PREDICTOR_SUB_DELTA_YEAR_PREF[] =
  "network.predictor.subresource-degradation.year";
const int32_t PREDICTOR_SUB_DELTA_YEAR_DEFAULT = 50;
const char PREDICTOR_SUB_DELTA_MAX_PREF[] =
  "network.predictor.subresource-degradation.max";
const int32_t PREDICTOR_SUB_DELTA_MAX_DEFAULT = 100;

const char PREDICTOR_PRECONNECT_MIN_PREF[] =
  "network.predictor.preconnect-min-confidence";
const int32_t PRECONNECT_MIN_DEFAULT = 90;
const char PREDICTOR_PRERESOLVE_MIN_PREF[] =
  "network.predictor.preresolve-min-confidence";
const int32_t PRERESOLVE_MIN_DEFAULT = 60;
const char PREDICTOR_REDIRECT_LIKELY_PREF[] =
  "network.predictor.redirect-likely-confidence";
const int32_t REDIRECT_LIKELY_DEFAULT = 75;

const char PREDICTOR_MAX_RESOURCES_PREF[] =
  "network.predictor.max-resources-per-entry";
const uint32_t PREDICTOR_MAX_RESOURCES_DEFAULT = 100;

// This is selected in concert with max-resources-per-entry to keep memory usage
// low-ish. The default of the combo of the two is ~50k
const char PREDICTOR_MAX_URI_LENGTH_PREF[] =
  "network.predictor.max-uri-length";
const uint32_t PREDICTOR_MAX_URI_LENGTH_DEFAULT = 500;

const char PREDICTOR_CLEANED_UP_PREF[] = "network.predictor.cleaned-up";

// All these time values are in sec
const uint32_t ONE_DAY = 86400U;
const uint32_t ONE_WEEK = 7U * ONE_DAY;
const uint32_t ONE_MONTH = 30U * ONE_DAY;
const uint32_t ONE_YEAR = 365U * ONE_DAY;

const uint32_t STARTUP_WINDOW = 5U * 60U; // 5min

// Version of metadata entries we expect
const uint32_t METADATA_VERSION = 1;

// ID Extensions for cache entries
#define PREDICTOR_ORIGIN_EXTENSION "predictor-origin"

// Get the full origin (scheme, host, port) out of a URI (maybe should be part
// of nsIURI instead?)
static nsresult
ExtractOrigin(nsIURI *uri, nsIURI **originUri, nsIIOService *ioService)
{
  nsAutoCString s;
  s.Truncate();
  nsresult rv = nsContentUtils::GetASCIIOrigin(uri, s);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewURI(originUri, s, nullptr, nullptr, ioService);
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

// Listener for the speculative DNS requests we'll fire off, which just ignores
// the result (since we're just trying to warm the cache). This also exists to
// reduce round-trips to the main thread, by being something threadsafe the
// Predictor can use.

NS_IMPL_ISUPPORTS(Predictor::DNSListener, nsIDNSListener);

NS_IMETHODIMP
Predictor::DNSListener::OnLookupComplete(nsICancelable *request,
                                         nsIDNSRecord *rec,
                                         nsresult status)
{
  return NS_OK;
}

// Class to proxy important information from the initial predictor call through
// the cache API and back into the internals of the predictor. We can't use the
// predictor itself, as it may have multiple actions in-flight, and each action
// has different parameters.
NS_IMPL_ISUPPORTS(Predictor::Action, nsICacheEntryOpenCallback);

Predictor::Action::Action(bool fullUri, bool predict,
                          Predictor::Reason reason,
                          nsIURI *targetURI, nsIURI *sourceURI,
                          nsINetworkPredictorVerifier *verifier,
                          Predictor *predictor)
  :mFullUri(fullUri)
  ,mPredict(predict)
  ,mTargetURI(targetURI)
  ,mSourceURI(sourceURI)
  ,mVerifier(verifier)
  ,mStackCount(0)
  ,mPredictor(predictor)
{
  mStartTime = TimeStamp::Now();
  if (mPredict) {
    mPredictReason = reason.mPredict;
  } else {
    mLearnReason = reason.mLearn;
  }
}

Predictor::Action::Action(bool fullUri, bool predict,
                          Predictor::Reason reason,
                          nsIURI *targetURI, nsIURI *sourceURI,
                          nsINetworkPredictorVerifier *verifier,
                          Predictor *predictor, uint8_t stackCount)
  :mFullUri(fullUri)
  ,mPredict(predict)
  ,mTargetURI(targetURI)
  ,mSourceURI(sourceURI)
  ,mVerifier(verifier)
  ,mStackCount(stackCount)
  ,mPredictor(predictor)
{
  mStartTime = TimeStamp::Now();
  if (mPredict) {
    mPredictReason = reason.mPredict;
  } else {
    mLearnReason = reason.mLearn;
  }
}

Predictor::Action::~Action()
{ }

NS_IMETHODIMP
Predictor::Action::OnCacheEntryCheck(nsICacheEntry *entry,
                                     nsIApplicationCache *appCache,
                                     uint32_t *result)
{
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::Action::OnCacheEntryAvailable(nsICacheEntry *entry, bool isNew,
                                         nsIApplicationCache *appCache,
                                         nsresult result)
{
  MOZ_ASSERT(NS_IsMainThread(), "Got cache entry off main thread!");

  nsAutoCString targetURI, sourceURI;
  mTargetURI->GetAsciiSpec(targetURI);
  if (mSourceURI) {
    mSourceURI->GetAsciiSpec(sourceURI);
  }
  PREDICTOR_LOG(("OnCacheEntryAvailable %p called. entry=%p mFullUri=%d mPredict=%d "
                 "mPredictReason=%d mLearnReason=%d mTargetURI=%s "
                 "mSourceURI=%s mStackCount=%d isNew=%d result=0x%08x",
                 this, entry, mFullUri, mPredict, mPredictReason, mLearnReason,
                 targetURI.get(), sourceURI.get(), mStackCount,
                 isNew, result));
  if (NS_FAILED(result)) {
    PREDICTOR_LOG(("OnCacheEntryAvailable %p FAILED to get cache entry (0x%08X). "
                   "Aborting.", this, result));
    return NS_OK;
  }
  Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_WAIT_TIME,
                                 mStartTime);
  if (mPredict) {
    bool predicted = mPredictor->PredictInternal(mPredictReason, entry, isNew,
                                                 mFullUri, mTargetURI,
                                                 mVerifier, mStackCount);
    Telemetry::AccumulateTimeDelta(
      Telemetry::PREDICTOR_PREDICT_WORK_TIME, mStartTime);
    if (predicted) {
      Telemetry::AccumulateTimeDelta(
        Telemetry::PREDICTOR_PREDICT_TIME_TO_ACTION, mStartTime);
    } else {
      Telemetry::AccumulateTimeDelta(
        Telemetry::PREDICTOR_PREDICT_TIME_TO_INACTION, mStartTime);
    }
  } else {
    mPredictor->LearnInternal(mLearnReason, entry, isNew, mFullUri, mTargetURI,
                              mSourceURI);
    Telemetry::AccumulateTimeDelta(
      Telemetry::PREDICTOR_LEARN_WORK_TIME, mStartTime);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(Predictor,
                  nsINetworkPredictor,
                  nsIObserver,
                  nsISpeculativeConnectionOverrider,
                  nsIInterfaceRequestor,
                  nsICacheEntryMetaDataVisitor,
                  nsINetworkPredictorVerifier)

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
  ,mMaxResourcesPerEntry(PREDICTOR_MAX_RESOURCES_DEFAULT)
  ,mStartupCount(1)
  ,mMaxURILength(PREDICTOR_MAX_URI_LENGTH_DEFAULT)
{
  MOZ_ASSERT(!sSelf, "multiple Predictor instances!");
  sSelf = this;
}

Predictor::~Predictor()
{
  if (mInitialized)
    Shutdown();

  sSelf = nullptr;
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

  Preferences::AddIntVarCache(&mMaxResourcesPerEntry,
                              PREDICTOR_MAX_RESOURCES_PREF,
                              PREDICTOR_MAX_RESOURCES_DEFAULT);

  Preferences::AddBoolVarCache(&mCleanedUp, PREDICTOR_CLEANED_UP_PREF, false);

  Preferences::AddUintVarCache(&mMaxURILength, PREDICTOR_MAX_URI_LENGTH_PREF,
                               PREDICTOR_MAX_URI_LENGTH_DEFAULT);

  if (!mCleanedUp) {
    mCleanupTimer = do_CreateInstance("@mozilla.org/timer;1");
    mCleanupTimer->Init(this, 60 * 1000, nsITimer::TYPE_ONE_SHOT);
  }

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

  if (mCleanupTimer) {
    mCleanupTimer->Cancel();
    mCleanupTimer = nullptr;
  }
}

NS_IMETHODIMP
Predictor::Observe(nsISupports *subject, const char *topic,
                   const char16_t *data_unicode)
{
  nsresult rv = NS_OK;
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor observing something off main thread!");

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    Shutdown();
  } else if (!strcmp("timer-callback", topic)) {
    MaybeCleanupOldDBFiles();
    mCleanupTimer = nullptr;
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

NS_IMETHODIMP
Predictor::GetAllow1918(bool *allow1918)
{
  *allow1918 = false;
  return NS_OK;
}

// Predictor::nsIInterfaceRequestor

NS_IMETHODIMP
Predictor::GetInterface(const nsIID &iid, void **result)
{
  return QueryInterface(iid, result);
}

#ifdef MOZ_NUWA_PROCESS
namespace {
class NuwaMarkPredictorThreadRunner : public nsRunnable
{
  NS_IMETHODIMP Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    if (IsNuwaProcess()) {
      NuwaMarkCurrentThread(nullptr, nullptr);
    }
    return NS_OK;
  }
};
} // namespace
#endif

// Predictor::nsICacheEntryMetaDataVisitor

#define SEEN_META_DATA "predictor::seen"
#define RESOURCE_META_DATA "predictor::resource-count"
#define META_DATA_PREFIX "predictor::"

static bool
IsURIMetadataElement(const char *key)
{
  return StringBeginsWith(nsDependentCString(key),
                          NS_LITERAL_CSTRING(META_DATA_PREFIX)) &&
         !NS_LITERAL_CSTRING(SEEN_META_DATA).Equals(key) &&
         !NS_LITERAL_CSTRING(RESOURCE_META_DATA).Equals(key);
}

nsresult
Predictor::OnMetaDataElement(const char *asciiKey, const char *asciiValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsURIMetadataElement(asciiKey)) {
    // This isn't a bit of metadata we care about
    return NS_OK;
  }

  nsCString key, value;
  key.AssignASCII(asciiKey);
  value.AssignASCII(asciiValue);
  mKeysToOperateOn.AppendElement(key);
  mValuesToOperateOn.AppendElement(value);

  return NS_OK;
}

// Predictor::nsINetworkPredictor

nsresult
Predictor::Init()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsNeckoChild());

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

  rv = InstallObserver();
  NS_ENSURE_SUCCESS(rv, rv);

  mLastStartupTime = mStartupTime = NOW_IN_SECONDS();

  if (!mDNSListener) {
    mDNSListener = new DNSListener();
  }

  nsCOMPtr<nsICacheStorageService> cacheStorageService =
    do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<LoadContextInfo> lci =
    new LoadContextInfo(false, false, NeckoOriginAttributes());

  rv = cacheStorageService->DiskCacheStorage(lci, false,
                                             getter_AddRefs(mCacheDiskStorage));
  NS_ENSURE_SUCCESS(rv, rv);

  mIOService = do_GetService("@mozilla.org/network/io-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(getter_AddRefs(mStartupURI),
                 "predictor://startup", nullptr, mIOService);
  NS_ENSURE_SUCCESS(rv, rv);

  mSpeculativeService = do_QueryInterface(mIOService, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = true;

  return rv;
}

namespace {
class PredictorThreadShutdownRunner : public nsRunnable
{
public:
  PredictorThreadShutdownRunner(nsIThread *ioThread, bool success)
    :mIOThread(ioThread)
    ,mSuccess(success)
  { }
  ~PredictorThreadShutdownRunner() { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "Shutting down io thread off main thread!");
    if (mSuccess) {
      // This means the cleanup happened. Mark so we don't try in the
      // future.
      Preferences::SetBool(PREDICTOR_CLEANED_UP_PREF, true);
    }
    return mIOThread->AsyncShutdown();
  }

private:
  nsCOMPtr<nsIThread> mIOThread;
  bool mSuccess;
};

class PredictorOldCleanupRunner : public nsRunnable
{
public:
  PredictorOldCleanupRunner(nsIThread *ioThread, nsIFile *dbFile)
    :mIOThread(ioThread)
    ,mDBFile(dbFile)
  { }

  ~PredictorOldCleanupRunner() { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Cleaning up old files on main thread!");
    nsresult rv = CheckForAndDeleteOldDBFiles();
    RefPtr<PredictorThreadShutdownRunner> runner =
      new PredictorThreadShutdownRunner(mIOThread, NS_SUCCEEDED(rv));
    NS_DispatchToMainThread(runner);
    return NS_OK;
  }

private:
  nsresult CheckForAndDeleteOldDBFiles()
  {
    nsCOMPtr<nsIFile> oldDBFile;
    nsresult rv = mDBFile->GetParent(getter_AddRefs(oldDBFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = oldDBFile->AppendNative(NS_LITERAL_CSTRING("seer.sqlite"));
    NS_ENSURE_SUCCESS(rv, rv);

    bool fileExists = false;
    rv = oldDBFile->Exists(&fileExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fileExists) {
      rv = oldDBFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    fileExists = false;
    rv = mDBFile->Exists(&fileExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fileExists) {
      rv = mDBFile->Remove(false);
    }

    return rv;
  }

  nsCOMPtr<nsIThread> mIOThread;
  nsCOMPtr<nsIFile> mDBFile;
};

} // namespace

void
Predictor::MaybeCleanupOldDBFiles()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEnabled || mCleanedUp) {
    return;
  }

  mCleanedUp = true;

  // This is used for cleaning up junk left over from the old backend
  // built on top of sqlite, if necessary.
  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  RETURN_IF_FAILED(rv);
  rv = dbFile->AppendNative(NS_LITERAL_CSTRING("netpredictions.sqlite"));
  RETURN_IF_FAILED(rv);

  nsCOMPtr<nsIThread> ioThread;
  rv = NS_NewNamedThread("NetPredictClean", getter_AddRefs(ioThread));
  RETURN_IF_FAILED(rv);

#ifdef MOZ_NUWA_PROCESS
  nsCOMPtr<nsIRunnable> nuwaRunner = new NuwaMarkPredictorThreadRunner();
  ioThread->Dispatch(nuwaRunner, NS_DISPATCH_NORMAL);
#endif

  RefPtr<PredictorOldCleanupRunner> runner =
    new PredictorOldCleanupRunner(ioThread, dbFile);
  ioThread->Dispatch(runner, NS_DISPATCH_NORMAL);
}

void
Predictor::Shutdown()
{
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "Predictor::Shutdown called off the main thread!");
    return;
  }

  RemoveObserver();

  mInitialized = false;
}

nsresult
Predictor::Create(nsISupports *aOuter, const nsIID& aIID,
                  void **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<Predictor> svc = new Predictor();
  if (IsNeckoChild()) {
    // Child threads only need to be call into the public interface methods
    // so we don't bother with initialization
    return svc->QueryInterface(aIID, aResult);
  }

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

// Called from the main thread to initiate predictive actions
NS_IMETHODIMP
Predictor::Predict(nsIURI *targetURI, nsIURI *sourceURI,
                   PredictorPredictReason reason, nsILoadContext *loadContext,
                   nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  PREDICTOR_LOG(("Predictor::Predict"));

  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(gNeckoChild);

    PREDICTOR_LOG(("    called on child process"));

    ipc::OptionalURIParams serTargetURI, serSourceURI;
    SerializeURI(targetURI, serTargetURI);
    SerializeURI(sourceURI, serSourceURI);

    IPC::SerializedLoadContext serLoadContext;
    serLoadContext.Init(loadContext);

    // If two different threads are predicting concurently, this will be
    // overwritten. Thankfully, we only use this in tests, which will
    // overwrite mVerifier perhaps multiple times for each individual test;
    // however, within each test, the multiple predict calls should have the
    // same verifier.
    if (verifier) {
      PREDICTOR_LOG(("    was given a verifier"));
      mChildVerifier = verifier;
    }
    PREDICTOR_LOG(("    forwarding to parent process"));
    gNeckoChild->SendPredPredict(serTargetURI, serSourceURI,
                                 reason, serLoadContext, verifier);
    return NS_OK;
  }

  PREDICTOR_LOG(("    called on parent process"));

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return NS_OK;
  }

  if (!mEnabled) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  if (loadContext && loadContext->UsePrivateBrowsing()) {
    // Don't want to do anything in PB mode
    PREDICTOR_LOG(("    in PB mode"));
    return NS_OK;
  }

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    // Nothing we can do for non-HTTP[S] schemes
    PREDICTOR_LOG(("    got non-http[s] URI"));
    return NS_OK;
  }

  // Ensure we've been given the appropriate arguments for the kind of
  // prediction we're being asked to do
  nsCOMPtr<nsIURI> uriKey = targetURI;
  nsCOMPtr<nsIURI> originKey;
  switch (reason) {
    case nsINetworkPredictor::PREDICT_LINK:
      if (!targetURI || !sourceURI) {
        PREDICTOR_LOG(("    link invalid URI state"));
        return NS_ERROR_INVALID_ARG;
      }
      // Link hover is a special case where we can predict without hitting the
      // db, so let's go ahead and fire off that prediction here.
      PredictForLink(targetURI, sourceURI, verifier);
      return NS_OK;
    case nsINetworkPredictor::PREDICT_LOAD:
      if (!targetURI || sourceURI) {
        PREDICTOR_LOG(("    load invalid URI state"));
        return NS_ERROR_INVALID_ARG;
      }
      break;
    case nsINetworkPredictor::PREDICT_STARTUP:
      if (targetURI || sourceURI) {
        PREDICTOR_LOG(("    startup invalid URI state"));
        return NS_ERROR_INVALID_ARG;
      }
      uriKey = mStartupURI;
      originKey = mStartupURI;
      break;
    default:
      PREDICTOR_LOG(("    invalid reason"));
      return NS_ERROR_INVALID_ARG;
  }

  Predictor::Reason argReason;
  argReason.mPredict = reason;

  // First we open the regular cache entry, to ensure we don't gum up the works
  // waiting on the less-important predictor-only cache entry
  RefPtr<Predictor::Action> uriAction =
    new Predictor::Action(Predictor::Action::IS_FULL_URI,
                          Predictor::Action::DO_PREDICT, argReason, targetURI,
                          nullptr, verifier, this);
  nsAutoCString uriKeyStr;
  uriKey->GetAsciiSpec(uriKeyStr);
  PREDICTOR_LOG(("    Predict uri=%s reason=%d action=%p", uriKeyStr.get(),
                 reason, uriAction.get()));
  uint32_t openFlags = nsICacheStorage::OPEN_READONLY |
                       nsICacheStorage::OPEN_SECRETLY |
                       nsICacheStorage::OPEN_PRIORITY |
                       nsICacheStorage::CHECK_MULTITHREADED;
  mCacheDiskStorage->AsyncOpenURI(uriKey, EmptyCString(), openFlags, uriAction);

  // Now we do the origin-only (and therefore predictor-only) entry
  nsCOMPtr<nsIURI> targetOrigin;
  nsresult rv = ExtractOrigin(uriKey, getter_AddRefs(targetOrigin), mIOService);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!originKey) {
    originKey = targetOrigin;
  }

  RefPtr<Predictor::Action> originAction =
    new Predictor::Action(Predictor::Action::IS_ORIGIN,
                          Predictor::Action::DO_PREDICT, argReason,
                          targetOrigin, nullptr, verifier, this);
  nsAutoCString originKeyStr;
  originKey->GetAsciiSpec(originKeyStr);
  PREDICTOR_LOG(("    Predict origin=%s reason=%d action=%p", originKeyStr.get(),
                 reason, originAction.get()));
  openFlags = nsICacheStorage::OPEN_READONLY |
              nsICacheStorage::OPEN_SECRETLY |
              nsICacheStorage::CHECK_MULTITHREADED;
  mCacheDiskStorage->AsyncOpenURI(originKey,
                                  NS_LITERAL_CSTRING(PREDICTOR_ORIGIN_EXTENSION),
                                  openFlags, originAction);

  PREDICTOR_LOG(("    predict returning"));
  return NS_OK;
}

bool
Predictor::PredictInternal(PredictorPredictReason reason, nsICacheEntry *entry,
                           bool isNew, bool fullUri, nsIURI *targetURI,
                           nsINetworkPredictorVerifier *verifier,
                           uint8_t stackCount)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictInternal"));
  bool rv = false;

  if (reason == nsINetworkPredictor::PREDICT_LOAD) {
    MaybeLearnForStartup(targetURI, fullUri);
  }

  if (isNew) {
    // nothing else we can do here
    PREDICTOR_LOG(("    new entry"));
    return rv;
  }

  switch (reason) {
    case nsINetworkPredictor::PREDICT_LOAD:
      rv = PredictForPageload(entry, stackCount, verifier);
      break;
    case nsINetworkPredictor::PREDICT_STARTUP:
      rv = PredictForStartup(entry, verifier);
      break;
    default:
      PREDICTOR_LOG(("    invalid reason"));
      MOZ_ASSERT(false, "Got unexpected value for prediction reason");
  }

  return rv;
}

void
Predictor::PredictForLink(nsIURI *targetURI, nsIURI *sourceURI,
                          nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictForLink"));
  if (!mSpeculativeService) {
    PREDICTOR_LOG(("    missing speculative service"));
    return;
  }

  if (!mEnableHoverOnSSL) {
    bool isSSL = false;
    sourceURI->SchemeIs("https", &isSSL);
    if (isSSL) {
      // We don't want to predict from an HTTPS page, to avoid info leakage
      PREDICTOR_LOG(("    Not predicting for link hover - on an SSL page"));
      return;
    }
  }

  mSpeculativeService->SpeculativeConnect(targetURI, nullptr);
  if (verifier) {
    PREDICTOR_LOG(("    sending verification"));
    verifier->OnPredictPreconnect(targetURI);
  }
}

// This is the driver for prediction based on a new pageload.
const uint8_t MAX_PAGELOAD_DEPTH = 10;
bool
Predictor::PredictForPageload(nsICacheEntry *entry, uint8_t stackCount,
                              nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictForPageload"));

  if (stackCount > MAX_PAGELOAD_DEPTH) {
    PREDICTOR_LOG(("    exceeded recursion depth!"));
    return false;
  }

  uint32_t lastLoad;
  nsresult rv = entry->GetLastFetched(&lastLoad);
  NS_ENSURE_SUCCESS(rv, false);

  int32_t globalDegradation = CalculateGlobalDegradation(lastLoad);

  int32_t loadCount;
  rv = entry->GetFetchCount(&loadCount);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIURI> redirectURI;
  if (WouldRedirect(entry, loadCount, lastLoad, globalDegradation,
                    getter_AddRefs(redirectURI))) {
    mPreconnects.AppendElement(redirectURI);
    Predictor::Reason reason;
    reason.mPredict = nsINetworkPredictor::PREDICT_LOAD;
    RefPtr<Predictor::Action> redirectAction =
      new Predictor::Action(Predictor::Action::IS_FULL_URI,
                            Predictor::Action::DO_PREDICT, reason, redirectURI,
                            nullptr, verifier, this, stackCount + 1);
    nsAutoCString redirectUriString;
    redirectURI->GetAsciiSpec(redirectUriString);
    PREDICTOR_LOG(("    Predict redirect uri=%s action=%p", redirectUriString.get(),
                   redirectAction.get()));
    uint32_t openFlags = nsICacheStorage::OPEN_READONLY |
                         nsICacheStorage::OPEN_SECRETLY |
                         nsICacheStorage::OPEN_PRIORITY |
                         nsICacheStorage::CHECK_MULTITHREADED;
    mCacheDiskStorage->AsyncOpenURI(redirectURI, EmptyCString(), openFlags,
                                    redirectAction);
    return RunPredictions(verifier);
  }

  CalculatePredictions(entry, lastLoad, loadCount, globalDegradation);

  return RunPredictions(verifier);
}

// This is the driver for predicting at browser startup time based on pages that
// have previously been loaded close to startup.
bool
Predictor::PredictForStartup(nsICacheEntry *entry,
                             nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictForStartup"));
  int32_t globalDegradation = CalculateGlobalDegradation(mLastStartupTime);
  CalculatePredictions(entry, mLastStartupTime, mStartupCount,
                       globalDegradation);
  return RunPredictions(verifier);
}

// This calculates how much to degrade our confidence in our data based on
// the last time this top-level resource was loaded. This "global degradation"
// applies to *all* subresources we have associated with the top-level
// resource. This will be in addition to any reduction in confidence we have
// associated with a particular subresource.
int32_t
Predictor::CalculateGlobalDegradation(uint32_t lastLoad)
{
  MOZ_ASSERT(NS_IsMainThread());

  int32_t globalDegradation;
  uint32_t delta = NOW_IN_SECONDS() - lastLoad;
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
// @param hitCount - the number of times we have loaded this subresource as part
//                   of this top-level load
// @param hitsPossible - the number of times we have performed this top-level
//                       load
// @param lastHit - the timestamp of the last time we loaded this subresource as
//                  part of this top-level load
// @param lastPossible - the timestamp of the last time we performed this
//                       top-level load
// @param globalDegradation - the degradation for this top-level load as
//                            determined by CalculateGlobalDegradation
int32_t
Predictor::CalculateConfidence(uint32_t hitCount, uint32_t hitsPossible,
                               uint32_t lastHit, uint32_t lastPossible,
                               int32_t globalDegradation)
{
  MOZ_ASSERT(NS_IsMainThread());

  Telemetry::AutoCounter<Telemetry::PREDICTOR_PREDICTIONS_CALCULATED> predictionsCalculated;
  ++predictionsCalculated;

  if (!hitsPossible) {
    return 0;
  }

  int32_t baseConfidence = (hitCount * 100) / hitsPossible;
  int32_t maxConfidence = 100;
  int32_t confidenceDegradation = 0;

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
  int32_t confidence = baseConfidence - confidenceDegradation - globalDegradation;
  confidence = std::max(confidence, 0);
  confidence = std::min(confidence, maxConfidence);

  Telemetry::Accumulate(Telemetry::PREDICTOR_BASE_CONFIDENCE, baseConfidence);
  Telemetry::Accumulate(Telemetry::PREDICTOR_SUBRESOURCE_DEGRADATION,
                        confidenceDegradation);
  Telemetry::Accumulate(Telemetry::PREDICTOR_CONFIDENCE, confidence);
  return confidence;
}

void
Predictor::CalculatePredictions(nsICacheEntry *entry, uint32_t lastLoad,
                                uint32_t loadCount, int32_t globalDegradation)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Since the visitor gets called under a cache lock, all we do there is get
  // copies of the keys/values we care about, and then do the real work here
  entry->VisitMetaData(this);
  nsTArray<nsCString> keysToOperateOn, valuesToOperateOn;
  keysToOperateOn.SwapElements(mKeysToOperateOn);
  valuesToOperateOn.SwapElements(mValuesToOperateOn);

  MOZ_ASSERT(keysToOperateOn.Length() == valuesToOperateOn.Length());
  for (size_t i = 0; i < keysToOperateOn.Length(); ++i) {
    const char *key = keysToOperateOn[i].BeginReading();
    const char *value = valuesToOperateOn[i].BeginReading();

    nsCOMPtr<nsIURI> uri;
    uint32_t hitCount, lastHit, flags;
    if (!ParseMetaDataEntry(key, value, getter_AddRefs(uri), hitCount, lastHit, flags)) {
      // This failed, get rid of it so we don't waste space
      entry->SetMetaDataElement(key, nullptr);
      continue;
    }

    int32_t confidence = CalculateConfidence(hitCount, loadCount, lastHit,
                                             lastLoad, globalDegradation);
    SetupPrediction(confidence, uri);
  }
}

// (Maybe) adds a predictive action to the prediction runner, based on our
// calculated confidence for the subresource in question.
void
Predictor::SetupPrediction(int32_t confidence, nsIURI *uri)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (confidence >= mPreconnectMinConfidence) {
    mPreconnects.AppendElement(uri);
  } else if (confidence >= mPreresolveMinConfidence) {
    mPreresolves.AppendElement(uri);
  }
}

// Runs predictions that have been set up.
bool
Predictor::RunPredictions(nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread(), "Running prediction off main thread");

  PREDICTOR_LOG(("Predictor::RunPredictions"));

  bool predicted = false;
  uint32_t len, i;

  nsTArray<nsCOMPtr<nsIURI>> preconnects, preresolves;
  preconnects.SwapElements(mPreconnects);
  preresolves.SwapElements(mPreresolves);

  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PREDICTIONS> totalPredictions;
  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS> totalPreconnects;
  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRERESOLVES> totalPreresolves;

  len = preconnects.Length();
  for (i = 0; i < len; ++i) {
    PREDICTOR_LOG(("    doing preconnect"));
    nsCOMPtr<nsIURI> uri = preconnects[i];
    ++totalPredictions;
    ++totalPreconnects;
    mSpeculativeService->SpeculativeConnect(uri, this);
    predicted = true;
    if (verifier) {
      PREDICTOR_LOG(("    sending preconnect verification"));
      verifier->OnPredictPreconnect(uri);
    }
  }

  len = preresolves.Length();
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  for (i = 0; i < len; ++i) {
    nsCOMPtr<nsIURI> uri = preresolves[i];
    ++totalPredictions;
    ++totalPreresolves;
    nsAutoCString hostname;
    uri->GetAsciiHost(hostname);
    PREDICTOR_LOG(("    doing preresolve %s", hostname.get()));
    nsCOMPtr<nsICancelable> tmpCancelable;
    mDnsService->AsyncResolve(hostname,
                              (nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
                               nsIDNSService::RESOLVE_SPECULATE),
                              mDNSListener, nullptr,
                              getter_AddRefs(tmpCancelable));
    predicted = true;
    if (verifier) {
      PREDICTOR_LOG(("    sending preresolve verification"));
      verifier->OnPredictDNS(uri);
    }
  }

  return predicted;
}

// Find out if a top-level page is likely to redirect.
bool
Predictor::WouldRedirect(nsICacheEntry *entry, uint32_t loadCount,
                         uint32_t lastLoad, int32_t globalDegradation,
                         nsIURI **redirectURI)
{
  // TODO - not doing redirects for first go around
  MOZ_ASSERT(NS_IsMainThread());

  return false;
}

// Called from the main thread to update the database
NS_IMETHODIMP
Predictor::Learn(nsIURI *targetURI, nsIURI *sourceURI,
                 PredictorLearnReason reason,
                 nsILoadContext *loadContext)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  PREDICTOR_LOG(("Predictor::Learn"));

  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(gNeckoChild);

    PREDICTOR_LOG(("    called on child process"));

    ipc::URIParams serTargetURI;
    SerializeURI(targetURI, serTargetURI);

    ipc::OptionalURIParams serSourceURI;
    SerializeURI(sourceURI, serSourceURI);

    IPC::SerializedLoadContext serLoadContext;
    serLoadContext.Init(loadContext);

    PREDICTOR_LOG(("    forwarding to parent"));
    gNeckoChild->SendPredLearn(serTargetURI, serSourceURI, reason,
                               serLoadContext);
    return NS_OK;
  }

  PREDICTOR_LOG(("    called on parent process"));

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return NS_OK;
  }

  if (!mEnabled) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  if (loadContext && loadContext->UsePrivateBrowsing()) {
    // Don't want to do anything in PB mode
    PREDICTOR_LOG(("    in PB mode"));
    return NS_OK;
  }

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    PREDICTOR_LOG(("    got non-HTTP[S] URI"));
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> targetOrigin;
  nsCOMPtr<nsIURI> sourceOrigin;
  nsCOMPtr<nsIURI> uriKey;
  nsCOMPtr<nsIURI> originKey;
  nsresult rv;

  switch (reason) {
  case nsINetworkPredictor::LEARN_LOAD_TOPLEVEL:
    if (!targetURI || sourceURI) {
      PREDICTOR_LOG(("    load toplevel invalid URI state"));
      return NS_ERROR_INVALID_ARG;
    }
    rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin), mIOService);
    NS_ENSURE_SUCCESS(rv, rv);
    uriKey = targetURI;
    originKey = targetOrigin;
    break;
  case nsINetworkPredictor::LEARN_STARTUP:
    if (!targetURI || sourceURI) {
      PREDICTOR_LOG(("    startup invalid URI state"));
      return NS_ERROR_INVALID_ARG;
    }
    rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin), mIOService);
    NS_ENSURE_SUCCESS(rv, rv);
    uriKey = mStartupURI;
    originKey = mStartupURI;
    break;
  case nsINetworkPredictor::LEARN_LOAD_REDIRECT:
  case nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE:
    if (!targetURI || !sourceURI) {
      PREDICTOR_LOG(("    redirect/subresource invalid URI state"));
      return NS_ERROR_INVALID_ARG;
    }
    rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin), mIOService);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ExtractOrigin(sourceURI, getter_AddRefs(sourceOrigin), mIOService);
    NS_ENSURE_SUCCESS(rv, rv);
    uriKey = sourceURI;
    originKey = sourceOrigin;
    break;
  default:
    PREDICTOR_LOG(("    invalid reason"));
    return NS_ERROR_INVALID_ARG;
  }

  Telemetry::AutoCounter<Telemetry::PREDICTOR_LEARN_ATTEMPTS> learnAttempts;
  ++learnAttempts;

  Predictor::Reason argReason;
  argReason.mLearn = reason;

  // We always open the full uri (general cache) entry first, so we don't gum up
  // the works waiting on predictor-only entries to open
  RefPtr<Predictor::Action> uriAction =
    new Predictor::Action(Predictor::Action::IS_FULL_URI,
                          Predictor::Action::DO_LEARN, argReason, targetURI,
                          sourceURI, nullptr, this);
  nsAutoCString uriKeyStr, targetUriStr, sourceUriStr;
  uriKey->GetAsciiSpec(uriKeyStr);
  targetURI->GetAsciiSpec(targetUriStr);
  if (sourceURI) {
    sourceURI->GetAsciiSpec(sourceUriStr);
  }
  PREDICTOR_LOG(("    Learn uriKey=%s targetURI=%s sourceURI=%s reason=%d "
                 "action=%p", uriKeyStr.get(), targetUriStr.get(),
                 sourceUriStr.get(), reason, uriAction.get()));
  // For learning full URI things, we *always* open readonly and secretly, as we
  // rely on actual pageloads to update the entry's metadata for us.
  uint32_t uriOpenFlags = nsICacheStorage::OPEN_READONLY |
                          nsICacheStorage::OPEN_SECRETLY |
                          nsICacheStorage::CHECK_MULTITHREADED;
  if (reason == nsINetworkPredictor::LEARN_LOAD_TOPLEVEL) {
    // Learning for toplevel we want to open the full uri entry priority, since
    // it's likely this entry will be used soon anyway, and we want this to be
    // opened ASAP.
    uriOpenFlags |= nsICacheStorage::OPEN_PRIORITY;
  }
  mCacheDiskStorage->AsyncOpenURI(uriKey, EmptyCString(), uriOpenFlags,
                                  uriAction);

  // Now we open the origin-only (and therefore predictor-only) entry
  RefPtr<Predictor::Action> originAction =
    new Predictor::Action(Predictor::Action::IS_ORIGIN,
                          Predictor::Action::DO_LEARN, argReason, targetOrigin,
                          sourceOrigin, nullptr, this);
  nsAutoCString originKeyStr, targetOriginStr, sourceOriginStr;
  originKey->GetAsciiSpec(originKeyStr);
  targetOrigin->GetAsciiSpec(targetOriginStr);
  if (sourceOrigin) {
    sourceOrigin->GetAsciiSpec(sourceOriginStr);
  }
  PREDICTOR_LOG(("    Learn originKey=%s targetOrigin=%s sourceOrigin=%s reason=%d "
                 "action=%p", originKeyStr.get(), targetOriginStr.get(),
                 sourceOriginStr.get(), reason, originAction.get()));
  uint32_t originOpenFlags;
  if (reason == nsINetworkPredictor::LEARN_LOAD_TOPLEVEL) {
    // This is the only case when we want to update the 'last used' metadata on
    // the cache entry we're getting. This only applies to predictor-specific
    // entries.
    originOpenFlags = nsICacheStorage::OPEN_NORMALLY |
                      nsICacheStorage::CHECK_MULTITHREADED;
  } else {
    originOpenFlags = nsICacheStorage::OPEN_READONLY |
                      nsICacheStorage::OPEN_SECRETLY |
                      nsICacheStorage::CHECK_MULTITHREADED;
  }
  mCacheDiskStorage->AsyncOpenURI(originKey,
                                  NS_LITERAL_CSTRING(PREDICTOR_ORIGIN_EXTENSION),
                                  originOpenFlags, originAction);

  PREDICTOR_LOG(("Predictor::Learn returning"));
  return NS_OK;
}

void
Predictor::LearnInternal(PredictorLearnReason reason, nsICacheEntry *entry,
                         bool isNew, bool fullUri, nsIURI *targetURI,
                         nsIURI *sourceURI)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::LearnInternal"));

  nsCString junk;
  if (!fullUri && reason == nsINetworkPredictor::LEARN_LOAD_TOPLEVEL &&
      NS_FAILED(entry->GetMetaDataElement(SEEN_META_DATA, getter_Copies(junk)))) {
    // This is an origin-only entry that we haven't seen before. Let's mark it
    // as seen.
    PREDICTOR_LOG(("    marking new origin entry as seen"));
    nsresult rv = entry->SetMetaDataElement(SEEN_META_DATA, "1");
    if (NS_FAILED(rv)) {
      PREDICTOR_LOG(("    failed to mark origin entry seen"));
      return;
    }

    // Need to ensure someone else can get to the entry if necessary
    entry->MetaDataReady();
    return;
  }

  switch (reason) {
    case nsINetworkPredictor::LEARN_LOAD_TOPLEVEL:
      // This actually has no work associated with it, since all we need to do
      // is update the timestamps and fetch count, and that's done for us by
      // opening the cache entry.
      PREDICTOR_LOG(("    nothing to do for toplevel"));
      break;
    case nsINetworkPredictor::LEARN_LOAD_REDIRECT:
      if (fullUri) {
        LearnForRedirect(entry, targetURI);
      }
      break;
    case nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE:
      LearnForSubresource(entry, targetURI);
      break;
    case nsINetworkPredictor::LEARN_STARTUP:
      LearnForStartup(entry, targetURI);
      break;
    default:
      PREDICTOR_LOG(("    unexpected reason value"));
      MOZ_ASSERT(false, "Got unexpected value for learn reason!");
  }
}

NS_IMPL_ISUPPORTS(Predictor::SpaceCleaner, nsICacheEntryMetaDataVisitor)

NS_IMETHODIMP
Predictor::SpaceCleaner::OnMetaDataElement(const char *key, const char *value)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsURIMetadataElement(key)) {
    // This isn't a bit of metadata we care about
    return NS_OK;
  }

  nsCOMPtr<nsIURI> parsedURI;
  uint32_t hitCount, lastHit, flags;
  bool ok = mPredictor->ParseMetaDataEntry(key, value,
                                           getter_AddRefs(parsedURI),
                                           hitCount, lastHit, flags);

  if (!ok) {
    // Couldn't parse this one, just get rid of it
    nsCString nsKey;
    nsKey.AssignASCII(key);
    mLongKeysToDelete.AppendElement(nsKey);
    return NS_OK;
  }

  nsCString uri;
  nsresult rv = parsedURI->GetAsciiSpec(uri);
  uint32_t uriLength = uri.Length();
  if (NS_SUCCEEDED(rv) &&
      uriLength > mPredictor->mMaxURILength) {
    // Default to getting rid of URIs that are too long and were put in before
    // we had our limit on URI length, in order to free up some space.
    nsCString nsKey;
    nsKey.AssignASCII(key);
    mLongKeysToDelete.AppendElement(nsKey);
    return NS_OK;
  }

  if (!mLRUKeyToDelete || lastHit < mLRUStamp) {
    mLRUKeyToDelete = key;
    mLRUStamp = lastHit;
  }

  return NS_OK;
}

void
Predictor::SpaceCleaner::Finalize(nsICacheEntry *entry)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mLRUKeyToDelete) {
    entry->SetMetaDataElement(mLRUKeyToDelete, nullptr);
  }

  for (size_t i = 0; i < mLongKeysToDelete.Length(); ++i) {
    entry->SetMetaDataElement(mLongKeysToDelete[i].BeginReading(), nullptr);
  }
}

// Called when a subresource has been hit from a top-level load. Uses the two
// helper functions above to update the database appropriately.
void
Predictor::LearnForSubresource(nsICacheEntry *entry, nsIURI *targetURI)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::LearnForSubresource"));

  uint32_t lastLoad;
  nsresult rv = entry->GetLastFetched(&lastLoad);
  RETURN_IF_FAILED(rv);

  int32_t loadCount;
  rv = entry->GetFetchCount(&loadCount);
  RETURN_IF_FAILED(rv);

  nsCString key;
  key.AssignLiteral(META_DATA_PREFIX);
  nsCString uri;
  targetURI->GetAsciiSpec(uri);
  key.Append(uri);
  if (uri.Length() > mMaxURILength) {
    // We do this to conserve space/prevent OOMs
    PREDICTOR_LOG(("    uri too long!"));
    entry->SetMetaDataElement(key.BeginReading(), nullptr);
    return;
  }

  nsCString value;
  rv = entry->GetMetaDataElement(key.BeginReading(), getter_Copies(value));

  uint32_t hitCount, lastHit, flags;
  bool isNewResource = (NS_FAILED(rv) ||
                        !ParseMetaDataEntry(nullptr, value.BeginReading(),
                                            nullptr, hitCount, lastHit, flags));

  int32_t resourceCount = 0;
  if (isNewResource) {
    // This is a new addition
    PREDICTOR_LOG(("    new resource"));
    nsCString s;
    rv = entry->GetMetaDataElement(RESOURCE_META_DATA, getter_Copies(s));
    if (NS_SUCCEEDED(rv)) {
      resourceCount = atoi(s.BeginReading());
    }
    if (resourceCount >= mMaxResourcesPerEntry) {
      RefPtr<Predictor::SpaceCleaner> cleaner =
        new Predictor::SpaceCleaner(this);
      entry->VisitMetaData(cleaner);
      cleaner->Finalize(entry);
    } else {
      ++resourceCount;
    }
    nsAutoCString count;
    count.AppendInt(resourceCount);
    rv = entry->SetMetaDataElement(RESOURCE_META_DATA, count.BeginReading());
    if (NS_FAILED(rv)) {
      PREDICTOR_LOG(("    failed to update resource count"));
      return;
    }
    hitCount = 1;
  } else {
    PREDICTOR_LOG(("    existing resource"));
    hitCount = std::min(hitCount + 1, static_cast<uint32_t>(loadCount));
  }

  nsCString newValue;
  newValue.AppendInt(METADATA_VERSION);
  newValue.AppendLiteral(",");
  newValue.AppendInt(hitCount);
  newValue.AppendLiteral(",");
  newValue.AppendInt(lastLoad);
  // These are for flags, that will be used for prefetch and possibly other
  // things later on
  newValue.AppendLiteral(",");
  newValue.AppendInt(0);
  rv = entry->SetMetaDataElement(key.BeginReading(), newValue.BeginReading());
  PREDICTOR_LOG(("    SetMetaDataElement -> 0x%08X", rv));
  if (NS_FAILED(rv) && isNewResource) {
    // Roll back the increment to the resource count we made above.
    PREDICTOR_LOG(("    rolling back resource count update"));
    --resourceCount;
    if (resourceCount == 0) {
      entry->SetMetaDataElement(RESOURCE_META_DATA, nullptr);
    } else {
      nsAutoCString count;
      count.AppendInt(resourceCount);
      entry->SetMetaDataElement(RESOURCE_META_DATA, count.BeginReading());
    }
  }
}

// This is called when a top-level loaded ended up redirecting to a different
// URI so we can keep track of that fact.
void
Predictor::LearnForRedirect(nsICacheEntry *entry, nsIURI *targetURI)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO - not doing redirects for first go around
  PREDICTOR_LOG(("Predictor::LearnForRedirect"));
}

// This will add a page to our list of startup pages if it's being loaded
// before our startup window has expired.
void
Predictor::MaybeLearnForStartup(nsIURI *uri, bool fullUri)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO - not doing startup for first go around
  PREDICTOR_LOG(("Predictor::MaybeLearnForStartup"));
}

// Add information about a top-level load to our list of startup pages
void
Predictor::LearnForStartup(nsICacheEntry *entry, nsIURI *targetURI)
{
  MOZ_ASSERT(NS_IsMainThread());

  // These actually do the same set of work, just on different entries, so we
  // can pass through to get the real work done here
  PREDICTOR_LOG(("Predictor::LearnForStartup"));
  LearnForSubresource(entry, targetURI);
}

bool
Predictor::ParseMetaDataEntry(const char *key, const char *value, nsIURI **uri,
                              uint32_t &hitCount, uint32_t &lastHit,
                              uint32_t &flags)
{
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::ParseMetaDataEntry key=%s value=%s",
                 key ? key : "", value));

  const char *comma = strchr(value, ',');
  if (!comma) {
    PREDICTOR_LOG(("    could not find first comma"));
    return false;
  }

  uint32_t version = static_cast<uint32_t>(atoi(value));
  PREDICTOR_LOG(("    version -> %u", version));

  if (version != METADATA_VERSION) {
    PREDICTOR_LOG(("    metadata version mismatch %u != %u", version,
                   METADATA_VERSION));
    return false;
  }

  value = comma + 1;
  comma = strchr(value, ',');
  if (!comma) {
    PREDICTOR_LOG(("    could not find second comma"));
    return false;
  }

  hitCount = static_cast<uint32_t>(atoi(value));
  PREDICTOR_LOG(("    hitCount -> %u", hitCount));

  value = comma + 1;
  comma = strchr(value, ',');
  if (!comma) {
    PREDICTOR_LOG(("    could not find third comma"));
    return false;
  }

  lastHit = static_cast<uint32_t>(atoi(value));
  PREDICTOR_LOG(("    lastHit -> %u", lastHit));

  value = comma + 1;
  flags = static_cast<uint32_t>(atoi(value));
  PREDICTOR_LOG(("    flags -> %u", flags));

  if (key) {
    const char *uriStart = key + (sizeof(META_DATA_PREFIX) - 1);
    nsresult rv = NS_NewURI(uri, uriStart, nullptr, mIOService);
    if (NS_FAILED(rv)) {
      PREDICTOR_LOG(("    NS_NewURI returned 0x%X", rv));
      return false;
    }
    PREDICTOR_LOG(("    uri -> %s", uriStart));
  }

  return true;
}

NS_IMETHODIMP
Predictor::Reset()
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  PREDICTOR_LOG(("Predictor::Reset"));

  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(gNeckoChild);

    PREDICTOR_LOG(("    forwarding to parent process"));
    gNeckoChild->SendPredReset();
    return NS_OK;
  }

  PREDICTOR_LOG(("    called on parent process"));

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return NS_OK;
  }

  if (!mEnabled) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  RefPtr<Predictor::Resetter> reset = new Predictor::Resetter(this);
  PREDICTOR_LOG(("    created a resetter"));
  mCacheDiskStorage->AsyncVisitStorage(reset, true);
  PREDICTOR_LOG(("    Cache async launched, returning now"));

  return NS_OK;
}

NS_IMPL_ISUPPORTS(Predictor::Resetter,
                  nsICacheEntryOpenCallback,
                  nsICacheEntryMetaDataVisitor,
                  nsICacheStorageVisitor);

Predictor::Resetter::Resetter(Predictor *predictor)
  :mEntriesToVisit(0)
  ,mPredictor(predictor)
{ }

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryCheck(nsICacheEntry *entry,
                                       nsIApplicationCache *appCache,
                                       uint32_t *result)
{
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryAvailable(nsICacheEntry *entry, bool isNew,
                                           nsIApplicationCache *appCache,
                                           nsresult result)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(result)) {
    // This can happen when we've tried to open an entry that doesn't exist for
    // some non-reset operation, and then get reset shortly thereafter (as
    // happens in some of our tests).
    --mEntriesToVisit;
    if (!mEntriesToVisit) {
      Complete();
    }
    return NS_OK;
  }

  entry->VisitMetaData(this);
  nsTArray<nsCString> keysToDelete;
  keysToDelete.SwapElements(mKeysToDelete);

  for (size_t i = 0; i < keysToDelete.Length(); ++i) {
    const char *key = keysToDelete[i].BeginReading();
    entry->SetMetaDataElement(key, nullptr);
  }

  --mEntriesToVisit;
  if (!mEntriesToVisit) {
    Complete();
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnMetaDataElement(const char *asciiKey,
                                       const char *asciiValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!StringBeginsWith(nsDependentCString(asciiKey),
                        NS_LITERAL_CSTRING(META_DATA_PREFIX))) {
    // Not a metadata entry we care about, carry on
    return NS_OK;
  }

  nsCString key;
  key.AssignASCII(asciiKey);
  mKeysToDelete.AppendElement(key);

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheStorageInfo(uint32_t entryCount, uint64_t consumption,
                                        uint64_t capacity, nsIFile *diskDirectory)
{
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryInfo(nsIURI *uri, const nsACString &idEnhance,
                                      int64_t dataSize, int32_t fetchCount,
                                      uint32_t lastModifiedTime, uint32_t expirationTime,
                                      bool aPinned)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The predictor will only ever touch entries with no idEnhance ("") or an
  // idEnhance of PREDICTOR_ORIGIN_EXTENSION, so we filter out any entries that
  // don't match that to avoid doing extra work.
  if (idEnhance.EqualsLiteral(PREDICTOR_ORIGIN_EXTENSION)) {
    // This is an entry we own, so we can just doom it entirely
    mPredictor->mCacheDiskStorage->AsyncDoomURI(uri, idEnhance, nullptr);
  } else if (idEnhance.IsEmpty()) {
    // This is an entry we don't own, so we have to be a little more careful and
    // just get rid of our own metadata entries. Append it to an array of things
    // to operate on and then do the operations later so we don't end up calling
    // Complete() multiple times/too soon.
    ++mEntriesToVisit;
    mURIsToVisit.AppendElement(uri);
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryVisitCompleted()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsCOMPtr<nsIURI>> urisToVisit;
  urisToVisit.SwapElements(mURIsToVisit);

  MOZ_ASSERT(mEntriesToVisit == urisToVisit.Length());
  if (!mEntriesToVisit) {
    Complete();
    return NS_OK;
  }

  uint32_t entriesToVisit = urisToVisit.Length();
  for (uint32_t i = 0; i < entriesToVisit; ++i) {
    nsCString u;
    urisToVisit[i]->GetAsciiSpec(u);
    mPredictor->mCacheDiskStorage->AsyncOpenURI(
        urisToVisit[i], EmptyCString(),
        nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY | nsICacheStorage::CHECK_MULTITHREADED,
        this);
  }

  return NS_OK;
}

void
Predictor::Resetter::Complete()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    PREDICTOR_LOG(("COULD NOT GET OBSERVER SERVICE!"));
    return;
  }

  obs->NotifyObservers(nullptr, "predictor-reset-complete", nullptr);
}

// Helper functions to make using the predictor easier from native code

static nsresult
EnsureGlobalPredictor(nsINetworkPredictor **aPredictor)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsINetworkPredictor> predictor =
    do_GetService("@mozilla.org/network/predictor;1",
                  &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  predictor.forget(aPredictor);
  return NS_OK;
}

nsresult
PredictorPredict(nsIURI *targetURI, nsIURI *sourceURI,
                 PredictorPredictReason reason, nsILoadContext *loadContext,
                 nsINetworkPredictorVerifier *verifier)
{
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

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

// nsINetworkPredictorVerifier

/**
 * Call through to the child's verifier (only during tests).
 */
NS_IMETHODIMP
Predictor::OnPredictPreconnect(nsIURI *aURI) {
  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(mChildVerifier);
    return mChildVerifier->OnPredictPreconnect(aURI);
  }

  MOZ_DIAGNOSTIC_ASSERT(gNeckoParent);

  ipc::URIParams serURI;
  SerializeURI(aURI, serURI);

  if (!gNeckoParent->SendPredOnPredictPreconnect(serURI)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

/**
 * Call through to the child's verifier (only during tests)
 */
NS_IMETHODIMP
Predictor::OnPredictDNS(nsIURI *aURI) {
  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(mChildVerifier);
    return mChildVerifier->OnPredictDNS(aURI);
  }

  MOZ_DIAGNOSTIC_ASSERT(gNeckoParent);

  ipc::URIParams serURI;
  SerializeURI(aURI, serURI);

  if (!gNeckoParent->SendPredOnPredictDNS(serURI)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
