/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "Predictor.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsICacheStorage.h"
#include "nsICachingChannel.h"
#include "nsICancelable.h"
#include "nsIChannel.h"
#include "nsContentUtils.h"
#include "nsIDNSService.h"
#include "mozilla/dom/Document.h"
#include "nsIFile.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsILoadContext.h"
#include "nsILoadContextInfo.h"
#include "nsILoadGroup.h"
#include "nsINetworkPredictorVerifier.h"
#include "nsIObserverService.h"
#include "nsISpeculativeConnect.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"

#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"

#include "LoadContextInfo.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "mozilla/net/NeckoChild.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/ClearOnShutdown.h"

#include "CacheControlParser.h"
#include "ReferrerInfo.h"

using namespace mozilla;

namespace mozilla {
namespace net {

Predictor* Predictor::sSelf = nullptr;

static LazyLogModule gPredictorLog("NetworkPredictor");

#define PREDICTOR_LOG(args) \
  MOZ_LOG(gPredictorLog, mozilla::LogLevel::Debug, args)

#define NOW_IN_SECONDS() static_cast<uint32_t>(PR_Now() / PR_USEC_PER_SEC)

// All these time values are in sec
static const uint32_t ONE_DAY = 86400U;
static const uint32_t ONE_WEEK = 7U * ONE_DAY;
static const uint32_t ONE_MONTH = 30U * ONE_DAY;
static const uint32_t ONE_YEAR = 365U * ONE_DAY;

static const uint32_t STARTUP_WINDOW = 5U * 60U;  // 5min

// Version of metadata entries we expect
static const uint32_t METADATA_VERSION = 1;

// Flags available in entries
// FLAG_PREFETCHABLE - we have determined that this item is eligible for
// prefetch
static const uint32_t FLAG_PREFETCHABLE = 1 << 0;

// We save 12 bits in the "flags" section of our metadata for actual flags, the
// rest are to keep track of a rolling count of which loads a resource has been
// used on to determine if we can prefetch that resource or not;
static const uint8_t kRollingLoadOffset = 12;
static const int32_t kMaxPrefetchRollingLoadCount = 20;
static const uint32_t kFlagsMask = ((1 << kRollingLoadOffset) - 1);

// ID Extensions for cache entries
#define PREDICTOR_ORIGIN_EXTENSION "predictor-origin"

// Get the full origin (scheme, host, port) out of a URI (maybe should be part
// of nsIURI instead?)
static nsresult ExtractOrigin(nsIURI* uri, nsIURI** originUri) {
  nsAutoCString s;
  nsresult rv = nsContentUtils::GetASCIIOrigin(uri, s);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewURI(originUri, s);
}

// All URIs we get passed *must* be http or https if they're not null. This
// helps ensure that.
static bool IsNullOrHttp(nsIURI* uri) {
  if (!uri) {
    return true;
  }

  return uri->SchemeIs("http") || uri->SchemeIs("https");
}

// Listener for the speculative DNS requests we'll fire off, which just ignores
// the result (since we're just trying to warm the cache). This also exists to
// reduce round-trips to the main thread, by being something threadsafe the
// Predictor can use.

NS_IMPL_ISUPPORTS(Predictor::DNSListener, nsIDNSListener);

NS_IMETHODIMP
Predictor::DNSListener::OnLookupComplete(nsICancelable* request,
                                         nsIDNSRecord* rec, nsresult status) {
  return NS_OK;
}

// Class to proxy important information from the initial predictor call through
// the cache API and back into the internals of the predictor. We can't use the
// predictor itself, as it may have multiple actions in-flight, and each action
// has different parameters.
NS_IMPL_ISUPPORTS(Predictor::Action, nsICacheEntryOpenCallback);

Predictor::Action::Action(bool fullUri, bool predict, Predictor::Reason reason,
                          nsIURI* targetURI, nsIURI* sourceURI,
                          nsINetworkPredictorVerifier* verifier,
                          Predictor* predictor)
    : mFullUri(fullUri),
      mPredict(predict),
      mTargetURI(targetURI),
      mSourceURI(sourceURI),
      mVerifier(verifier),
      mStackCount(0),
      mPredictor(predictor) {
  mStartTime = TimeStamp::Now();
  if (mPredict) {
    mPredictReason = reason.mPredict;
  } else {
    mLearnReason = reason.mLearn;
  }
}

Predictor::Action::Action(bool fullUri, bool predict, Predictor::Reason reason,
                          nsIURI* targetURI, nsIURI* sourceURI,
                          nsINetworkPredictorVerifier* verifier,
                          Predictor* predictor, uint8_t stackCount)
    : mFullUri(fullUri),
      mPredict(predict),
      mTargetURI(targetURI),
      mSourceURI(sourceURI),
      mVerifier(verifier),
      mStackCount(stackCount),
      mPredictor(predictor) {
  mStartTime = TimeStamp::Now();
  if (mPredict) {
    mPredictReason = reason.mPredict;
  } else {
    mLearnReason = reason.mLearn;
  }
}

NS_IMETHODIMP
Predictor::Action::OnCacheEntryCheck(nsICacheEntry* entry, uint32_t* result) {
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::Action::OnCacheEntryAvailable(nsICacheEntry* entry, bool isNew,
                                         nsresult result) {
  MOZ_ASSERT(NS_IsMainThread(), "Got cache entry off main thread!");

  nsAutoCString targetURI, sourceURI;
  mTargetURI->GetAsciiSpec(targetURI);
  if (mSourceURI) {
    mSourceURI->GetAsciiSpec(sourceURI);
  }
  PREDICTOR_LOG(
      ("OnCacheEntryAvailable %p called. entry=%p mFullUri=%d mPredict=%d "
       "mPredictReason=%d mLearnReason=%d mTargetURI=%s "
       "mSourceURI=%s mStackCount=%d isNew=%d result=0x%08" PRIx32,
       this, entry, mFullUri, mPredict, mPredictReason, mLearnReason,
       targetURI.get(), sourceURI.get(), mStackCount, isNew,
       static_cast<uint32_t>(result)));
  if (NS_FAILED(result)) {
    PREDICTOR_LOG(
        ("OnCacheEntryAvailable %p FAILED to get cache entry (0x%08" PRIX32
         "). Aborting.",
         this, static_cast<uint32_t>(result)));
    return NS_OK;
  }
  Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_WAIT_TIME, mStartTime);
  if (mPredict) {
    bool predicted =
        mPredictor->PredictInternal(mPredictReason, entry, isNew, mFullUri,
                                    mTargetURI, mVerifier, mStackCount);
    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_PREDICT_WORK_TIME,
                                   mStartTime);
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
    Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_LEARN_WORK_TIME,
                                   mStartTime);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(Predictor, nsINetworkPredictor, nsIObserver,
                  nsISpeculativeConnectionOverrider, nsIInterfaceRequestor,
                  nsICacheEntryMetaDataVisitor, nsINetworkPredictorVerifier)

Predictor::Predictor()

{
  MOZ_ASSERT(!sSelf, "multiple Predictor instances!");
  sSelf = this;
}

Predictor::~Predictor() {
  if (mInitialized) Shutdown();

  sSelf = nullptr;
}

// Predictor::nsIObserver

nsresult Predictor::InstallObserver() {
  MOZ_ASSERT(NS_IsMainThread(), "Installing observer off main thread");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

void Predictor::RemoveObserver() {
  MOZ_ASSERT(NS_IsMainThread(), "Removing observer off main thread");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
}

NS_IMETHODIMP
Predictor::Observe(nsISupports* subject, const char* topic,
                   const char16_t* data_unicode) {
  nsresult rv = NS_OK;
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor observing something off main thread!");

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    Shutdown();
  }

  return rv;
}

// Predictor::nsISpeculativeConnectionOverrider

NS_IMETHODIMP
Predictor::GetIgnoreIdle(bool* ignoreIdle) {
  *ignoreIdle = true;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetParallelSpeculativeConnectLimit(
    uint32_t* parallelSpeculativeConnectLimit) {
  *parallelSpeculativeConnectLimit = 6;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetIsFromPredictor(bool* isFromPredictor) {
  *isFromPredictor = true;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::GetAllow1918(bool* allow1918) {
  *allow1918 = false;
  return NS_OK;
}

// Predictor::nsIInterfaceRequestor

NS_IMETHODIMP
Predictor::GetInterface(const nsIID& iid, void** result) {
  return QueryInterface(iid, result);
}

// Predictor::nsICacheEntryMetaDataVisitor

#define SEEN_META_DATA "predictor::seen"
#define RESOURCE_META_DATA "predictor::resource-count"
#define META_DATA_PREFIX "predictor::"

static bool IsURIMetadataElement(const char* key) {
  return StringBeginsWith(nsDependentCString(key),
                          nsLiteralCString(META_DATA_PREFIX)) &&
         !nsLiteralCString(SEEN_META_DATA).Equals(key) &&
         !nsLiteralCString(RESOURCE_META_DATA).Equals(key);
}

nsresult Predictor::OnMetaDataElement(const char* asciiKey,
                                      const char* asciiValue) {
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

nsresult Predictor::Init() {
  MOZ_DIAGNOSTIC_ASSERT(!IsNeckoChild());

  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "Predictor::Init called off the main thread!");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;

  rv = InstallObserver();
  NS_ENSURE_SUCCESS(rv, rv);

  mLastStartupTime = mStartupTime = NOW_IN_SECONDS();

  if (!mDNSListener) {
    mDNSListener = new DNSListener();
  }

  mCacheStorageService =
      do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mSpeculativeService = do_GetService("@mozilla.org/network/io-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(getter_AddRefs(mStartupURI), "predictor://startup");
  NS_ENSURE_SUCCESS(rv, rv);

  mDnsService = do_GetService("@mozilla.org/network/dns-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = true;

  return rv;
}

namespace {
class PredictorLearnRunnable final : public Runnable {
 public:
  PredictorLearnRunnable(nsIURI* targetURI, nsIURI* sourceURI,
                         PredictorLearnReason reason,
                         const OriginAttributes& oa)
      : Runnable("PredictorLearnRunnable"),
        mTargetURI(targetURI),
        mSourceURI(sourceURI),
        mReason(reason),
        mOA(oa) {
    MOZ_DIAGNOSTIC_ASSERT(targetURI, "Must have a target URI");
  }

  ~PredictorLearnRunnable() = default;

  NS_IMETHOD Run() override {
    if (!gNeckoChild) {
      // This may have gone away between when this runnable was dispatched and
      // when it actually runs, so let's be safe here, even though we asserted
      // earlier.
      PREDICTOR_LOG(("predictor::learn (async) gNeckoChild went away"));
      return NS_OK;
    }

    PREDICTOR_LOG(("predictor::learn (async) forwarding to parent"));
    gNeckoChild->SendPredLearn(mTargetURI, mSourceURI, mReason, mOA);

    return NS_OK;
  }

 private:
  nsCOMPtr<nsIURI> mTargetURI;
  nsCOMPtr<nsIURI> mSourceURI;
  PredictorLearnReason mReason;
  const OriginAttributes mOA;
};

}  // namespace

void Predictor::Shutdown() {
  if (!NS_IsMainThread()) {
    MOZ_ASSERT(false, "Predictor::Shutdown called off the main thread!");
    return;
  }

  RemoveObserver();

  mInitialized = false;
}

nsresult Predictor::Create(nsISupports* aOuter, const nsIID& aIID,
                           void** aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<Predictor> svc = new Predictor();
  if (IsNeckoChild()) {
    NeckoChild::InitNeckoChild();

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

NS_IMETHODIMP
Predictor::Predict(nsIURI* targetURI, nsIURI* sourceURI,
                   PredictorPredictReason reason,
                   JS::HandleValue originAttributes,
                   nsINetworkPredictorVerifier* verifier, JSContext* aCx) {
  OriginAttributes attrs;

  if (!originAttributes.isObject() || !attrs.Init(aCx, originAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return PredictNative(targetURI, sourceURI, reason, attrs, verifier);
}

// Called from the main thread to initiate predictive actions
NS_IMETHODIMP
Predictor::PredictNative(nsIURI* targetURI, nsIURI* sourceURI,
                         PredictorPredictReason reason,
                         const OriginAttributes& originAttributes,
                         nsINetworkPredictorVerifier* verifier) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  PREDICTOR_LOG(("Predictor::Predict"));

  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(gNeckoChild);

    PREDICTOR_LOG(("    called on child process"));
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
    gNeckoChild->SendPredPredict(targetURI, sourceURI, reason, originAttributes,
                                 verifier);
    return NS_OK;
  }

  PREDICTOR_LOG(("    called on parent process"));

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return NS_OK;
  }

  if (!StaticPrefs::network_predictor_enabled()) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  if (originAttributes.mPrivateBrowsingId > 0) {
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
      PredictForLink(targetURI, sourceURI, originAttributes, verifier);
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

  Predictor::Reason argReason{};
  argReason.mPredict = reason;

  // First we open the regular cache entry, to ensure we don't gum up the works
  // waiting on the less-important predictor-only cache entry
  RefPtr<Predictor::Action> uriAction = new Predictor::Action(
      Predictor::Action::IS_FULL_URI, Predictor::Action::DO_PREDICT, argReason,
      targetURI, nullptr, verifier, this);
  nsAutoCString uriKeyStr;
  uriKey->GetAsciiSpec(uriKeyStr);
  PREDICTOR_LOG(("    Predict uri=%s reason=%d action=%p", uriKeyStr.get(),
                 reason, uriAction.get()));

  nsCOMPtr<nsICacheStorage> cacheDiskStorage;

  RefPtr<LoadContextInfo> lci = new LoadContextInfo(false, originAttributes);

  nsresult rv = mCacheStorageService->DiskCacheStorage(
      lci, getter_AddRefs(cacheDiskStorage));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t openFlags =
      nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY |
      nsICacheStorage::OPEN_PRIORITY | nsICacheStorage::CHECK_MULTITHREADED;
  cacheDiskStorage->AsyncOpenURI(uriKey, ""_ns, openFlags, uriAction);

  // Now we do the origin-only (and therefore predictor-only) entry
  nsCOMPtr<nsIURI> targetOrigin;
  rv = ExtractOrigin(uriKey, getter_AddRefs(targetOrigin));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!originKey) {
    originKey = targetOrigin;
  }

  RefPtr<Predictor::Action> originAction = new Predictor::Action(
      Predictor::Action::IS_ORIGIN, Predictor::Action::DO_PREDICT, argReason,
      targetOrigin, nullptr, verifier, this);
  nsAutoCString originKeyStr;
  originKey->GetAsciiSpec(originKeyStr);
  PREDICTOR_LOG(("    Predict origin=%s reason=%d action=%p",
                 originKeyStr.get(), reason, originAction.get()));
  openFlags = nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY |
              nsICacheStorage::CHECK_MULTITHREADED;
  cacheDiskStorage->AsyncOpenURI(originKey,
                                 nsLiteralCString(PREDICTOR_ORIGIN_EXTENSION),
                                 openFlags, originAction);

  PREDICTOR_LOG(("    predict returning"));
  return NS_OK;
}

bool Predictor::PredictInternal(PredictorPredictReason reason,
                                nsICacheEntry* entry, bool isNew, bool fullUri,
                                nsIURI* targetURI,
                                nsINetworkPredictorVerifier* verifier,
                                uint8_t stackCount) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictInternal"));
  bool rv = false;

  nsCOMPtr<nsILoadContextInfo> lci;
  entry->GetLoadContextInfo(getter_AddRefs(lci));

  if (!lci) {
    return rv;
  }

  if (reason == nsINetworkPredictor::PREDICT_LOAD) {
    MaybeLearnForStartup(targetURI, fullUri, *lci->OriginAttributesPtr());
  }

  if (isNew) {
    // nothing else we can do here
    PREDICTOR_LOG(("    new entry"));
    return rv;
  }

  switch (reason) {
    case nsINetworkPredictor::PREDICT_LOAD:
      rv = PredictForPageload(entry, targetURI, stackCount, fullUri, verifier);
      break;
    case nsINetworkPredictor::PREDICT_STARTUP:
      rv = PredictForStartup(entry, fullUri, verifier);
      break;
    default:
      PREDICTOR_LOG(("    invalid reason"));
      MOZ_ASSERT(false, "Got unexpected value for prediction reason");
  }

  return rv;
}

void Predictor::PredictForLink(nsIURI* targetURI, nsIURI* sourceURI,
                               const OriginAttributes& originAttributes,
                               nsINetworkPredictorVerifier* verifier) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictForLink"));
  if (!mSpeculativeService) {
    PREDICTOR_LOG(("    missing speculative service"));
    return;
  }

  if (!StaticPrefs::network_predictor_enable_hover_on_ssl()) {
    if (sourceURI->SchemeIs("https")) {
      // We don't want to predict from an HTTPS page, to avoid info leakage
      PREDICTOR_LOG(("    Not predicting for link hover - on an SSL page"));
      return;
    }
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(targetURI, originAttributes);

  mSpeculativeService->SpeculativeConnect(targetURI, principal, nullptr);
  if (verifier) {
    PREDICTOR_LOG(("    sending verification"));
    verifier->OnPredictPreconnect(targetURI);
  }
}

// This is the driver for prediction based on a new pageload.
static const uint8_t MAX_PAGELOAD_DEPTH = 10;
bool Predictor::PredictForPageload(nsICacheEntry* entry, nsIURI* targetURI,
                                   uint8_t stackCount, bool fullUri,
                                   nsINetworkPredictorVerifier* verifier) {
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
  PREDICTOR_LOG(("    globalDegradation = %d", globalDegradation));

  int32_t loadCount;
  rv = entry->GetFetchCount(&loadCount);
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsILoadContextInfo> lci;

  rv = entry->GetLoadContextInfo(getter_AddRefs(lci));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsIURI> redirectURI;
  if (WouldRedirect(entry, loadCount, lastLoad, globalDegradation,
                    getter_AddRefs(redirectURI))) {
    mPreconnects.AppendElement(redirectURI);
    Predictor::Reason reason{};
    reason.mPredict = nsINetworkPredictor::PREDICT_LOAD;
    RefPtr<Predictor::Action> redirectAction = new Predictor::Action(
        Predictor::Action::IS_FULL_URI, Predictor::Action::DO_PREDICT, reason,
        redirectURI, nullptr, verifier, this, stackCount + 1);
    nsAutoCString redirectUriString;
    redirectURI->GetAsciiSpec(redirectUriString);

    nsCOMPtr<nsICacheStorage> cacheDiskStorage;

    rv = mCacheStorageService->DiskCacheStorage(
        lci, getter_AddRefs(cacheDiskStorage));
    NS_ENSURE_SUCCESS(rv, false);

    PREDICTOR_LOG(("    Predict redirect uri=%s action=%p",
                   redirectUriString.get(), redirectAction.get()));
    uint32_t openFlags =
        nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY |
        nsICacheStorage::OPEN_PRIORITY | nsICacheStorage::CHECK_MULTITHREADED;
    cacheDiskStorage->AsyncOpenURI(redirectURI, ""_ns, openFlags,
                                   redirectAction);
    return RunPredictions(nullptr, *lci->OriginAttributesPtr(), verifier);
  }

  CalculatePredictions(entry, targetURI, lastLoad, loadCount, globalDegradation,
                       fullUri);

  return RunPredictions(targetURI, *lci->OriginAttributesPtr(), verifier);
}

// This is the driver for predicting at browser startup time based on pages that
// have previously been loaded close to startup.
bool Predictor::PredictForStartup(nsICacheEntry* entry, bool fullUri,
                                  nsINetworkPredictorVerifier* verifier) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::PredictForStartup"));

  nsCOMPtr<nsILoadContextInfo> lci;

  nsresult rv = entry->GetLoadContextInfo(getter_AddRefs(lci));
  NS_ENSURE_SUCCESS(rv, false);

  int32_t globalDegradation = CalculateGlobalDegradation(mLastStartupTime);
  CalculatePredictions(entry, nullptr, mLastStartupTime, mStartupCount,
                       globalDegradation, fullUri);
  return RunPredictions(nullptr, *lci->OriginAttributesPtr(), verifier);
}

// This calculates how much to degrade our confidence in our data based on
// the last time this top-level resource was loaded. This "global degradation"
// applies to *all* subresources we have associated with the top-level
// resource. This will be in addition to any reduction in confidence we have
// associated with a particular subresource.
int32_t Predictor::CalculateGlobalDegradation(uint32_t lastLoad) {
  MOZ_ASSERT(NS_IsMainThread());

  int32_t globalDegradation;
  uint32_t delta = NOW_IN_SECONDS() - lastLoad;
  if (delta < ONE_DAY) {
    globalDegradation = StaticPrefs::network_predictor_page_degradation_day();
  } else if (delta < ONE_WEEK) {
    globalDegradation = StaticPrefs::network_predictor_page_degradation_week();
  } else if (delta < ONE_MONTH) {
    globalDegradation = StaticPrefs::network_predictor_page_degradation_month();
  } else if (delta < ONE_YEAR) {
    globalDegradation = StaticPrefs::network_predictor_page_degradation_year();
  } else {
    globalDegradation = StaticPrefs::network_predictor_page_degradation_max();
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
int32_t Predictor::CalculateConfidence(uint32_t hitCount, uint32_t hitsPossible,
                                       uint32_t lastHit, uint32_t lastPossible,
                                       int32_t globalDegradation) {
  MOZ_ASSERT(NS_IsMainThread());

  Telemetry::AutoCounter<Telemetry::PREDICTOR_PREDICTIONS_CALCULATED>
      predictionsCalculated;
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
    maxConfidence =
        StaticPrefs::network_predictor_preconnect_min_confidence() - 1;

    // Now calculate how much we want to degrade our confidence based on how
    // long it's been between the last time we did this top-level load and the
    // last time this top-level load included this subresource.
    PRTime delta = lastPossible - lastHit;
    if (delta == 0) {
      confidenceDegradation = 0;
    } else if (delta < ONE_DAY) {
      confidenceDegradation =
          StaticPrefs::network_predictor_subresource_degradation_day();
    } else if (delta < ONE_WEEK) {
      confidenceDegradation =
          StaticPrefs::network_predictor_subresource_degradation_week();
    } else if (delta < ONE_MONTH) {
      confidenceDegradation =
          StaticPrefs::network_predictor_subresource_degradation_month();
    } else if (delta < ONE_YEAR) {
      confidenceDegradation =
          StaticPrefs::network_predictor_subresource_degradation_year();
    } else {
      confidenceDegradation =
          StaticPrefs::network_predictor_subresource_degradation_max();
      maxConfidence = 0;
    }
  }

  // Calculate our confidence and clamp it to between 0 and maxConfidence
  // (<= 100)
  int32_t confidence =
      baseConfidence - confidenceDegradation - globalDegradation;
  confidence = std::max(confidence, 0);
  confidence = std::min(confidence, maxConfidence);

  Telemetry::Accumulate(Telemetry::PREDICTOR_BASE_CONFIDENCE, baseConfidence);
  Telemetry::Accumulate(Telemetry::PREDICTOR_SUBRESOURCE_DEGRADATION,
                        confidenceDegradation);
  Telemetry::Accumulate(Telemetry::PREDICTOR_CONFIDENCE, confidence);
  return confidence;
}

static void MakeMetadataEntry(const uint32_t hitCount, const uint32_t lastHit,
                              const uint32_t flags, nsCString& newValue) {
  newValue.Truncate();
  newValue.AppendInt(METADATA_VERSION);
  newValue.Append(',');
  newValue.AppendInt(hitCount);
  newValue.Append(',');
  newValue.AppendInt(lastHit);
  newValue.Append(',');
  newValue.AppendInt(flags);
}

// On every page load, the rolling window gets shifted by one bit, leaving the
// lowest bit at 0, to indicate that the subresource in question has not been
// seen on the most recent page load. If, at some point later during the page
// load, the subresource is seen again, we will then set the lowest bit to 1.
// This is how we keep track of how many of the last n pageloads (for n <= 20) a
// particular subresource has been seen. The rolling window is kept in the upper
// 20 bits of the flags element of the metadata. This saves 12 bits for regular
// old flags.
void Predictor::UpdateRollingLoadCount(nsICacheEntry* entry,
                                       const uint32_t flags, const char* key,
                                       const uint32_t hitCount,
                                       const uint32_t lastHit) {
  // Extract just the rolling load count from the flags, shift it to clear the
  // lowest bit, and put the new value with the existing flags.
  uint32_t rollingLoadCount = flags & ~kFlagsMask;
  rollingLoadCount <<= 1;
  uint32_t newFlags = (flags & kFlagsMask) | rollingLoadCount;

  // Finally, update the metadata on the cache entry.
  nsAutoCString newValue;
  MakeMetadataEntry(hitCount, lastHit, newFlags, newValue);
  entry->SetMetaDataElement(key, newValue.BeginReading());
}

uint32_t Predictor::ClampedPrefetchRollingLoadCount() {
  int32_t n = StaticPrefs::network_predictor_prefetch_rolling_load_count();
  if (n < 0) {
    return 0;
  }
  if (n > kMaxPrefetchRollingLoadCount) {
    return kMaxPrefetchRollingLoadCount;
  }
  return n;
}

void Predictor::CalculatePredictions(nsICacheEntry* entry, nsIURI* referrer,
                                     uint32_t lastLoad, uint32_t loadCount,
                                     int32_t globalDegradation, bool fullUri) {
  MOZ_ASSERT(NS_IsMainThread());

  // Since the visitor gets called under a cache lock, all we do there is get
  // copies of the keys/values we care about, and then do the real work here
  entry->VisitMetaData(this);
  nsTArray<nsCString> keysToOperateOn = std::move(mKeysToOperateOn),
                      valuesToOperateOn = std::move(mValuesToOperateOn);

  MOZ_ASSERT(keysToOperateOn.Length() == valuesToOperateOn.Length());
  for (size_t i = 0; i < keysToOperateOn.Length(); ++i) {
    const char* key = keysToOperateOn[i].BeginReading();
    const char* value = valuesToOperateOn[i].BeginReading();

    nsCString uri;
    uint32_t hitCount, lastHit, flags;
    if (!ParseMetaDataEntry(key, value, uri, hitCount, lastHit, flags)) {
      // This failed, get rid of it so we don't waste space
      entry->SetMetaDataElement(key, nullptr);
      continue;
    }

    int32_t confidence = CalculateConfidence(hitCount, loadCount, lastHit,
                                             lastLoad, globalDegradation);
    if (fullUri) {
      UpdateRollingLoadCount(entry, flags, key, hitCount, lastHit);
    }
    PREDICTOR_LOG(("CalculatePredictions key=%s value=%s confidence=%d", key,
                   value, confidence));
    PrefetchIgnoreReason reason = PREFETCH_OK;
    if (!fullUri) {
      // Not full URI - don't prefetch! No sense in it!
      PREDICTOR_LOG(("    forcing non-cacheability - not full URI"));
      if (flags & FLAG_PREFETCHABLE) {
        // This only applies if we had somehow otherwise marked this
        // prefetchable.
        reason = NOT_FULL_URI;
      }
      flags &= ~FLAG_PREFETCHABLE;
    } else if (!referrer) {
      // No referrer means we can't prefetch, so pretend it's non-cacheable,
      // no matter what.
      PREDICTOR_LOG(("    forcing non-cacheability - no referrer"));
      if (flags & FLAG_PREFETCHABLE) {
        // This only applies if we had somehow otherwise marked this
        // prefetchable.
        reason = NO_REFERRER;
      }
      flags &= ~FLAG_PREFETCHABLE;
    } else {
      uint32_t expectedRollingLoadCount =
          (1 << ClampedPrefetchRollingLoadCount()) - 1;
      expectedRollingLoadCount <<= kRollingLoadOffset;
      if ((flags & expectedRollingLoadCount) != expectedRollingLoadCount) {
        PREDICTOR_LOG(("    forcing non-cacheability - missed a load"));
        if (flags & FLAG_PREFETCHABLE) {
          // This only applies if we had somehow otherwise marked this
          // prefetchable.
          reason = MISSED_A_LOAD;
        }
        flags &= ~FLAG_PREFETCHABLE;
      }
    }

    PREDICTOR_LOG(("    setting up prediction"));
    SetupPrediction(confidence, flags, uri, reason);
  }
}

// (Maybe) adds a predictive action to the prediction runner, based on our
// calculated confidence for the subresource in question.
void Predictor::SetupPrediction(int32_t confidence, uint32_t flags,
                                const nsCString& uri,
                                PrefetchIgnoreReason earlyReason) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_OK;
  PREDICTOR_LOG(
      ("SetupPrediction enable-prefetch=%d prefetch-min-confidence=%d "
       "preconnect-min-confidence=%d preresolve-min-confidence=%d "
       "flags=%d confidence=%d uri=%s",
       StaticPrefs::network_predictor_enable_prefetch(),
       StaticPrefs::network_predictor_prefetch_min_confidence(),
       StaticPrefs::network_predictor_preconnect_min_confidence(),
       StaticPrefs::network_predictor_preresolve_min_confidence(), flags,
       confidence, uri.get()));

  bool prefetchOk = !!(flags & FLAG_PREFETCHABLE);
  PrefetchIgnoreReason reason = earlyReason;
  if (prefetchOk && !StaticPrefs::network_predictor_enable_prefetch()) {
    prefetchOk = false;
    reason = PREFETCH_DISABLED;
  } else if (prefetchOk && !ClampedPrefetchRollingLoadCount() &&
             confidence <
                 StaticPrefs::network_predictor_prefetch_min_confidence()) {
    prefetchOk = false;
    if (!ClampedPrefetchRollingLoadCount()) {
      reason = PREFETCH_DISABLED_VIA_COUNT;
    } else {
      reason = CONFIDENCE_TOO_LOW;
    }
  }

  // prefetchOk == false and reason == PREFETCH_OK indicates that the reason
  // we aren't prefetching this item is because it was marked un-prefetchable in
  // our metadata. We already have separate telemetry on that decision, so we
  // aren't going to accumulate more here. Right now we only care about why
  // something we had marked prefetchable isn't being prefetched.
  if (!prefetchOk && reason != PREFETCH_OK) {
    Telemetry::Accumulate(Telemetry::PREDICTOR_PREFETCH_IGNORE_REASON, reason);
  }

  if (prefetchOk) {
    nsCOMPtr<nsIURI> prefetchURI;
    rv = NS_NewURI(getter_AddRefs(prefetchURI), uri);
    if (NS_SUCCEEDED(rv)) {
      mPrefetches.AppendElement(prefetchURI);
    }
  } else if (confidence >=
             StaticPrefs::network_predictor_preconnect_min_confidence()) {
    nsCOMPtr<nsIURI> preconnectURI;
    rv = NS_NewURI(getter_AddRefs(preconnectURI), uri);
    if (NS_SUCCEEDED(rv)) {
      mPreconnects.AppendElement(preconnectURI);
    }
  } else if (confidence >=
             StaticPrefs::network_predictor_preresolve_min_confidence()) {
    nsCOMPtr<nsIURI> preresolveURI;
    rv = NS_NewURI(getter_AddRefs(preresolveURI), uri);
    if (NS_SUCCEEDED(rv)) {
      mPreresolves.AppendElement(preresolveURI);
    }
  }

  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    NS_NewURI returned 0x%" PRIx32, static_cast<uint32_t>(rv)));
  }
}

nsresult Predictor::Prefetch(nsIURI* uri, nsIURI* referrer,
                             const OriginAttributes& originAttributes,
                             nsINetworkPredictorVerifier* verifier) {
  nsAutoCString strUri, strReferrer;
  uri->GetAsciiSpec(strUri);
  referrer->GetAsciiSpec(strReferrer);
  PREDICTOR_LOG(("Predictor::Prefetch uri=%s referrer=%s verifier=%p",
                 strUri.get(), strReferrer.get(), verifier));
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(
      getter_AddRefs(channel), uri, nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER, nullptr, /* nsICookieJarSettings */
      nullptr,                               /* aPerformanceStorage */
      nullptr,                               /* aLoadGroup */
      nullptr,                               /* aCallbacks */
      nsIRequest::LOAD_BACKGROUND);

  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    NS_NewChannel failed rv=0x%" PRIX32, static_cast<uint32_t>(rv)));
    return rv;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  rv = loadInfo->SetOriginAttributes(originAttributes);

  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    Set originAttributes into loadInfo failed rv=0x%" PRIX32,
         static_cast<uint32_t>(rv)));
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel;
  httpChannel = do_QueryInterface(channel);
  if (!httpChannel) {
    PREDICTOR_LOG(("    Could not get HTTP Channel from new channel!"));
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo = new dom::ReferrerInfo(referrer);
  rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
  NS_ENSURE_SUCCESS(rv, rv);
  // XXX - set a header here to indicate this is a prefetch?

  nsCOMPtr<nsIStreamListener> listener =
      new PrefetchListener(verifier, uri, this);
  PREDICTOR_LOG(("    calling AsyncOpen listener=%p channel=%p", listener.get(),
                 channel.get()));
  rv = channel->AsyncOpen(listener);
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    AsyncOpen failed rv=0x%" PRIX32, static_cast<uint32_t>(rv)));
  }

  return rv;
}

// Runs predictions that have been set up.
bool Predictor::RunPredictions(nsIURI* referrer,
                               const OriginAttributes& originAttributes,
                               nsINetworkPredictorVerifier* verifier) {
  MOZ_ASSERT(NS_IsMainThread(), "Running prediction off main thread");

  PREDICTOR_LOG(("Predictor::RunPredictions"));

  bool predicted = false;
  uint32_t len, i;

  nsTArray<nsCOMPtr<nsIURI>> prefetches = std::move(mPrefetches),
                             preconnects = std::move(mPreconnects),
                             preresolves = std::move(mPreresolves);

  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PREDICTIONS>
      totalPredictions;
  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PREFETCHES> totalPrefetches;
  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRECONNECTS>
      totalPreconnects;
  Telemetry::AutoCounter<Telemetry::PREDICTOR_TOTAL_PRERESOLVES>
      totalPreresolves;

  len = prefetches.Length();
  for (i = 0; i < len; ++i) {
    PREDICTOR_LOG(("    doing prefetch"));
    nsCOMPtr<nsIURI> uri = prefetches[i];
    if (NS_SUCCEEDED(Prefetch(uri, referrer, originAttributes, verifier))) {
      ++totalPredictions;
      ++totalPrefetches;
      predicted = true;
    }
  }

  len = preconnects.Length();
  for (i = 0; i < len; ++i) {
    PREDICTOR_LOG(("    doing preconnect"));
    nsCOMPtr<nsIURI> uri = preconnects[i];
    ++totalPredictions;
    ++totalPreconnects;
    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(uri, originAttributes);
    mSpeculativeService->SpeculativeConnect(uri, principal, this);
    predicted = true;
    if (verifier) {
      PREDICTOR_LOG(("    sending preconnect verification"));
      verifier->OnPredictPreconnect(uri);
    }
  }

  len = preresolves.Length();
  for (i = 0; i < len; ++i) {
    nsCOMPtr<nsIURI> uri = preresolves[i];
    ++totalPredictions;
    ++totalPreresolves;
    nsAutoCString hostname;
    uri->GetAsciiHost(hostname);
    PREDICTOR_LOG(("    doing preresolve %s", hostname.get()));
    nsCOMPtr<nsICancelable> tmpCancelable;
    mDnsService->AsyncResolveNative(
        hostname, nsIDNSService::RESOLVE_TYPE_DEFAULT,
        (nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
         nsIDNSService::RESOLVE_SPECULATE),
        nullptr, mDNSListener, nullptr, originAttributes,
        getter_AddRefs(tmpCancelable));

    // Fetch HTTPS RR if needed.
    if (StaticPrefs::network_dns_upgrade_with_https_rr() ||
        StaticPrefs::network_dns_use_https_rr_as_altsvc()) {
      mDnsService->AsyncResolveNative(
          hostname, nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
          (nsIDNSService::RESOLVE_PRIORITY_MEDIUM |
           nsIDNSService::RESOLVE_SPECULATE),
          nullptr, mDNSListener, nullptr, originAttributes,
          getter_AddRefs(tmpCancelable));
    }

    predicted = true;
    if (verifier) {
      PREDICTOR_LOG(("    sending preresolve verification"));
      verifier->OnPredictDNS(uri);
    }
  }

  return predicted;
}

// Find out if a top-level page is likely to redirect.
bool Predictor::WouldRedirect(nsICacheEntry* entry, uint32_t loadCount,
                              uint32_t lastLoad, int32_t globalDegradation,
                              nsIURI** redirectURI) {
  // TODO - not doing redirects for first go around
  MOZ_ASSERT(NS_IsMainThread());

  return false;
}

NS_IMETHODIMP
Predictor::Learn(nsIURI* targetURI, nsIURI* sourceURI,
                 PredictorLearnReason reason, JS::HandleValue originAttributes,
                 JSContext* aCx) {
  OriginAttributes attrs;

  if (!originAttributes.isObject() || !attrs.Init(aCx, originAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return LearnNative(targetURI, sourceURI, reason, attrs);
}

// Called from the main thread to update the database
NS_IMETHODIMP
Predictor::LearnNative(nsIURI* targetURI, nsIURI* sourceURI,
                       PredictorLearnReason reason,
                       const OriginAttributes& originAttributes) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Predictor interface methods must be called on the main thread");

  PREDICTOR_LOG(("Predictor::Learn"));

  if (IsNeckoChild()) {
    MOZ_DIAGNOSTIC_ASSERT(gNeckoChild);

    PREDICTOR_LOG(("    called on child process"));

    RefPtr<PredictorLearnRunnable> runnable = new PredictorLearnRunnable(
        targetURI, sourceURI, reason, originAttributes);
    SchedulerGroup::Dispatch(TaskCategory::Other, runnable.forget());

    return NS_OK;
  }

  PREDICTOR_LOG(("    called on parent process"));

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return NS_OK;
  }

  if (!StaticPrefs::network_predictor_enabled()) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  if (originAttributes.mPrivateBrowsingId > 0) {
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
      rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin));
      NS_ENSURE_SUCCESS(rv, rv);
      uriKey = targetURI;
      originKey = targetOrigin;
      break;
    case nsINetworkPredictor::LEARN_STARTUP:
      if (!targetURI || sourceURI) {
        PREDICTOR_LOG(("    startup invalid URI state"));
        return NS_ERROR_INVALID_ARG;
      }
      rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin));
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
      rv = ExtractOrigin(targetURI, getter_AddRefs(targetOrigin));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = ExtractOrigin(sourceURI, getter_AddRefs(sourceOrigin));
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

  Predictor::Reason argReason{};
  argReason.mLearn = reason;

  // We always open the full uri (general cache) entry first, so we don't gum up
  // the works waiting on predictor-only entries to open
  RefPtr<Predictor::Action> uriAction = new Predictor::Action(
      Predictor::Action::IS_FULL_URI, Predictor::Action::DO_LEARN, argReason,
      targetURI, sourceURI, nullptr, this);
  nsAutoCString uriKeyStr, targetUriStr, sourceUriStr;
  uriKey->GetAsciiSpec(uriKeyStr);
  targetURI->GetAsciiSpec(targetUriStr);
  if (sourceURI) {
    sourceURI->GetAsciiSpec(sourceUriStr);
  }
  PREDICTOR_LOG(
      ("    Learn uriKey=%s targetURI=%s sourceURI=%s reason=%d "
       "action=%p",
       uriKeyStr.get(), targetUriStr.get(), sourceUriStr.get(), reason,
       uriAction.get()));

  nsCOMPtr<nsICacheStorage> cacheDiskStorage;

  RefPtr<LoadContextInfo> lci = new LoadContextInfo(false, originAttributes);

  rv = mCacheStorageService->DiskCacheStorage(lci,
                                              getter_AddRefs(cacheDiskStorage));
  NS_ENSURE_SUCCESS(rv, rv);

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
  cacheDiskStorage->AsyncOpenURI(uriKey, ""_ns, uriOpenFlags, uriAction);

  // Now we open the origin-only (and therefore predictor-only) entry
  RefPtr<Predictor::Action> originAction = new Predictor::Action(
      Predictor::Action::IS_ORIGIN, Predictor::Action::DO_LEARN, argReason,
      targetOrigin, sourceOrigin, nullptr, this);
  nsAutoCString originKeyStr, targetOriginStr, sourceOriginStr;
  originKey->GetAsciiSpec(originKeyStr);
  targetOrigin->GetAsciiSpec(targetOriginStr);
  if (sourceOrigin) {
    sourceOrigin->GetAsciiSpec(sourceOriginStr);
  }
  PREDICTOR_LOG(
      ("    Learn originKey=%s targetOrigin=%s sourceOrigin=%s reason=%d "
       "action=%p",
       originKeyStr.get(), targetOriginStr.get(), sourceOriginStr.get(), reason,
       originAction.get()));
  uint32_t originOpenFlags;
  if (reason == nsINetworkPredictor::LEARN_LOAD_TOPLEVEL) {
    // This is the only case when we want to update the 'last used' metadata on
    // the cache entry we're getting. This only applies to predictor-specific
    // entries.
    originOpenFlags =
        nsICacheStorage::OPEN_NORMALLY | nsICacheStorage::CHECK_MULTITHREADED;
  } else {
    originOpenFlags = nsICacheStorage::OPEN_READONLY |
                      nsICacheStorage::OPEN_SECRETLY |
                      nsICacheStorage::CHECK_MULTITHREADED;
  }
  cacheDiskStorage->AsyncOpenURI(originKey,
                                 nsLiteralCString(PREDICTOR_ORIGIN_EXTENSION),
                                 originOpenFlags, originAction);

  PREDICTOR_LOG(("Predictor::Learn returning"));
  return NS_OK;
}

void Predictor::LearnInternal(PredictorLearnReason reason, nsICacheEntry* entry,
                              bool isNew, bool fullUri, nsIURI* targetURI,
                              nsIURI* sourceURI) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::LearnInternal"));

  nsCString junk;
  if (!fullUri && reason == nsINetworkPredictor::LEARN_LOAD_TOPLEVEL &&
      NS_FAILED(
          entry->GetMetaDataElement(SEEN_META_DATA, getter_Copies(junk)))) {
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
      // This case only exists to be used during tests - code outside the
      // predictor tests should NEVER call Learn with LEARN_LOAD_TOPLEVEL.
      // The predictor xpcshell test needs this branch, however, because we
      // have no real page loads in xpcshell, and this is how we fake it up
      // so that all the work that normally happens behind the scenes in a
      // page load can be done for testing purposes.
      if (fullUri && StaticPrefs::network_predictor_doing_tests()) {
        PREDICTOR_LOG(
            ("    WARNING - updating rolling load count. "
             "If you see this outside tests, you did it wrong"));

        // Since the visitor gets called under a cache lock, all we do there is
        // get copies of the keys/values we care about, and then do the real
        // work here
        entry->VisitMetaData(this);
        nsTArray<nsCString> keysToOperateOn = std::move(mKeysToOperateOn),
                            valuesToOperateOn = std::move(mValuesToOperateOn);

        MOZ_ASSERT(keysToOperateOn.Length() == valuesToOperateOn.Length());
        for (size_t i = 0; i < keysToOperateOn.Length(); ++i) {
          const char* key = keysToOperateOn[i].BeginReading();
          const char* value = valuesToOperateOn[i].BeginReading();

          nsCString uri;
          uint32_t hitCount, lastHit, flags;
          if (!ParseMetaDataEntry(key, value, uri, hitCount, lastHit, flags)) {
            // This failed, get rid of it so we don't waste space
            entry->SetMetaDataElement(key, nullptr);
            continue;
          }
          UpdateRollingLoadCount(entry, flags, key, hitCount, lastHit);
        }
      } else {
        PREDICTOR_LOG(("    nothing to do for toplevel"));
      }
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
Predictor::SpaceCleaner::OnMetaDataElement(const char* key, const char* value) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsURIMetadataElement(key)) {
    // This isn't a bit of metadata we care about
    return NS_OK;
  }

  nsCString uri;
  uint32_t hitCount, lastHit, flags;
  bool ok =
      mPredictor->ParseMetaDataEntry(key, value, uri, hitCount, lastHit, flags);

  if (!ok) {
    // Couldn't parse this one, just get rid of it
    nsCString nsKey;
    nsKey.AssignASCII(key);
    mLongKeysToDelete.AppendElement(nsKey);
    return NS_OK;
  }

  uint32_t uriLength = uri.Length();
  if (uriLength > StaticPrefs::network_predictor_max_uri_length()) {
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

void Predictor::SpaceCleaner::Finalize(nsICacheEntry* entry) {
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
void Predictor::LearnForSubresource(nsICacheEntry* entry, nsIURI* targetURI) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(("Predictor::LearnForSubresource"));

  uint32_t lastLoad;
  nsresult rv = entry->GetLastFetched(&lastLoad);
  NS_ENSURE_SUCCESS_VOID(rv);

  int32_t loadCount;
  rv = entry->GetFetchCount(&loadCount);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCString key;
  key.AssignLiteral(META_DATA_PREFIX);
  nsCString uri;
  targetURI->GetAsciiSpec(uri);
  key.Append(uri);
  if (uri.Length() > StaticPrefs::network_predictor_max_uri_length()) {
    // We do this to conserve space/prevent OOMs
    PREDICTOR_LOG(("    uri too long!"));
    entry->SetMetaDataElement(key.BeginReading(), nullptr);
    return;
  }

  nsCString value;
  rv = entry->GetMetaDataElement(key.BeginReading(), getter_Copies(value));

  uint32_t hitCount, lastHit, flags;
  bool isNewResource =
      (NS_FAILED(rv) ||
       !ParseMetaDataEntry(key.BeginReading(), value.BeginReading(), uri,
                           hitCount, lastHit, flags));

  int32_t resourceCount = 0;
  if (isNewResource) {
    // This is a new addition
    PREDICTOR_LOG(("    new resource"));
    nsCString s;
    rv = entry->GetMetaDataElement(RESOURCE_META_DATA, getter_Copies(s));
    if (NS_SUCCEEDED(rv)) {
      resourceCount = atoi(s.BeginReading());
    }
    if (resourceCount >=
        StaticPrefs::network_predictor_max_resources_per_entry()) {
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
    flags = 0;
  } else {
    PREDICTOR_LOG(("    existing resource"));
    hitCount = std::min(hitCount + 1, static_cast<uint32_t>(loadCount));
  }

  // Update the rolling load count to mark this sub-resource as seen on the
  // most-recent pageload so it can be eligible for prefetch (assuming all
  // the other stars align).
  flags |= (1 << kRollingLoadOffset);

  nsCString newValue;
  MakeMetadataEntry(hitCount, lastLoad, flags, newValue);
  rv = entry->SetMetaDataElement(key.BeginReading(), newValue.BeginReading());
  PREDICTOR_LOG(
      ("    SetMetaDataElement -> 0x%08" PRIX32, static_cast<uint32_t>(rv)));
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
void Predictor::LearnForRedirect(nsICacheEntry* entry, nsIURI* targetURI) {
  MOZ_ASSERT(NS_IsMainThread());

  // TODO - not doing redirects for first go around
  PREDICTOR_LOG(("Predictor::LearnForRedirect"));
}

// This will add a page to our list of startup pages if it's being loaded
// before our startup window has expired.
void Predictor::MaybeLearnForStartup(nsIURI* uri, bool fullUri,
                                     const OriginAttributes& originAttributes) {
  MOZ_ASSERT(NS_IsMainThread());

  // TODO - not doing startup for first go around
  PREDICTOR_LOG(("Predictor::MaybeLearnForStartup"));
}

// Add information about a top-level load to our list of startup pages
void Predictor::LearnForStartup(nsICacheEntry* entry, nsIURI* targetURI) {
  MOZ_ASSERT(NS_IsMainThread());

  // These actually do the same set of work, just on different entries, so we
  // can pass through to get the real work done here
  PREDICTOR_LOG(("Predictor::LearnForStartup"));
  LearnForSubresource(entry, targetURI);
}

bool Predictor::ParseMetaDataEntry(const char* key, const char* value,
                                   nsCString& uri, uint32_t& hitCount,
                                   uint32_t& lastHit, uint32_t& flags) {
  MOZ_ASSERT(NS_IsMainThread());

  PREDICTOR_LOG(
      ("Predictor::ParseMetaDataEntry key=%s value=%s", key ? key : "", value));

  const char* comma = strchr(value, ',');
  if (!comma) {
    PREDICTOR_LOG(("    could not find first comma"));
    return false;
  }

  uint32_t version = static_cast<uint32_t>(atoi(value));
  PREDICTOR_LOG(("    version -> %u", version));

  if (version != METADATA_VERSION) {
    PREDICTOR_LOG(
        ("    metadata version mismatch %u != %u", version, METADATA_VERSION));
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
    const char* uriStart = key + (sizeof(META_DATA_PREFIX) - 1);
    uri.AssignASCII(uriStart);
    PREDICTOR_LOG(("    uri -> %s", uriStart));
  } else {
    uri.Truncate();
  }

  return true;
}

NS_IMETHODIMP
Predictor::Reset() {
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

  if (!StaticPrefs::network_predictor_enabled()) {
    PREDICTOR_LOG(("    not enabled"));
    return NS_OK;
  }

  RefPtr<Predictor::Resetter> reset = new Predictor::Resetter(this);
  PREDICTOR_LOG(("    created a resetter"));
  mCacheStorageService->AsyncVisitAllStorages(reset, true);
  PREDICTOR_LOG(("    Cache async launched, returning now"));

  return NS_OK;
}

NS_IMPL_ISUPPORTS(Predictor::Resetter, nsICacheEntryOpenCallback,
                  nsICacheEntryMetaDataVisitor, nsICacheStorageVisitor);

Predictor::Resetter::Resetter(Predictor* predictor)
    : mEntriesToVisit(0), mPredictor(predictor) {}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryCheck(nsICacheEntry* entry, uint32_t* result) {
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryAvailable(nsICacheEntry* entry, bool isNew,
                                           nsresult result) {
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
  nsTArray<nsCString> keysToDelete = std::move(mKeysToDelete);

  for (size_t i = 0; i < keysToDelete.Length(); ++i) {
    const char* key = keysToDelete[i].BeginReading();
    entry->SetMetaDataElement(key, nullptr);
  }

  --mEntriesToVisit;
  if (!mEntriesToVisit) {
    Complete();
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnMetaDataElement(const char* asciiKey,
                                       const char* asciiValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StringBeginsWith(nsDependentCString(asciiKey),
                        nsLiteralCString(META_DATA_PREFIX))) {
    // Not a metadata entry we care about, carry on
    return NS_OK;
  }

  nsCString key;
  key.AssignASCII(asciiKey);
  mKeysToDelete.AppendElement(key);

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheStorageInfo(uint32_t entryCount,
                                        uint64_t consumption, uint64_t capacity,
                                        nsIFile* diskDirectory) {
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryInfo(nsIURI* uri, const nsACString& idEnhance,
                                      int64_t dataSize, int32_t fetchCount,
                                      uint32_t lastModifiedTime,
                                      uint32_t expirationTime, bool aPinned,
                                      nsILoadContextInfo* aInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  // The predictor will only ever touch entries with no idEnhance ("") or an
  // idEnhance of PREDICTOR_ORIGIN_EXTENSION, so we filter out any entries that
  // don't match that to avoid doing extra work.
  if (idEnhance.EqualsLiteral(PREDICTOR_ORIGIN_EXTENSION)) {
    // This is an entry we own, so we can just doom it entirely
    nsCOMPtr<nsICacheStorage> cacheDiskStorage;

    rv = mPredictor->mCacheStorageService->DiskCacheStorage(
        aInfo, getter_AddRefs(cacheDiskStorage));

    NS_ENSURE_SUCCESS(rv, rv);
    cacheDiskStorage->AsyncDoomURI(uri, idEnhance, nullptr);
  } else if (idEnhance.IsEmpty()) {
    // This is an entry we don't own, so we have to be a little more careful and
    // just get rid of our own metadata entries. Append it to an array of things
    // to operate on and then do the operations later so we don't end up calling
    // Complete() multiple times/too soon.
    ++mEntriesToVisit;
    mURIsToVisit.AppendElement(uri);
    mInfosToVisit.AppendElement(aInfo);
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::Resetter::OnCacheEntryVisitCompleted() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  nsTArray<nsCOMPtr<nsIURI>> urisToVisit = std::move(mURIsToVisit);

  MOZ_ASSERT(mEntriesToVisit == urisToVisit.Length());

  nsTArray<nsCOMPtr<nsILoadContextInfo>> infosToVisit =
      std::move(mInfosToVisit);

  MOZ_ASSERT(mEntriesToVisit == infosToVisit.Length());

  if (!mEntriesToVisit) {
    Complete();
    return NS_OK;
  }

  uint32_t entriesToVisit = urisToVisit.Length();
  for (uint32_t i = 0; i < entriesToVisit; ++i) {
    nsCString u;
    nsCOMPtr<nsICacheStorage> cacheDiskStorage;

    rv = mPredictor->mCacheStorageService->DiskCacheStorage(
        infosToVisit[i], getter_AddRefs(cacheDiskStorage));
    NS_ENSURE_SUCCESS(rv, rv);

    urisToVisit[i]->GetAsciiSpec(u);
    rv = cacheDiskStorage->AsyncOpenURI(
        urisToVisit[i], ""_ns,
        nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY |
            nsICacheStorage::CHECK_MULTITHREADED,
        this);
    if (NS_FAILED(rv)) {
      mEntriesToVisit--;
      if (!mEntriesToVisit) {
        Complete();
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

void Predictor::Resetter::Complete() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    PREDICTOR_LOG(("COULD NOT GET OBSERVER SERVICE!"));
    return;
  }

  obs->NotifyObservers(nullptr, "predictor-reset-complete", nullptr);
}

// Helper functions to make using the predictor easier from native code

static StaticRefPtr<nsINetworkPredictor> sPredictor;

static nsresult EnsureGlobalPredictor(nsINetworkPredictor** aPredictor) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sPredictor) {
    nsresult rv;
    nsCOMPtr<nsINetworkPredictor> predictor =
        do_GetService("@mozilla.org/network/predictor;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    sPredictor = predictor;
    ClearOnShutdown(&sPredictor);
  }

  nsCOMPtr<nsINetworkPredictor> predictor = sPredictor.get();
  predictor.forget(aPredictor);
  return NS_OK;
}

nsresult PredictorPredict(nsIURI* targetURI, nsIURI* sourceURI,
                          PredictorPredictReason reason,
                          const OriginAttributes& originAttributes,
                          nsINetworkPredictorVerifier* verifier) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  return predictor->PredictNative(targetURI, sourceURI, reason,
                                  originAttributes, verifier);
}

nsresult PredictorLearn(nsIURI* targetURI, nsIURI* sourceURI,
                        PredictorLearnReason reason,
                        const OriginAttributes& originAttributes) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  return predictor->LearnNative(targetURI, sourceURI, reason, originAttributes);
}

nsresult PredictorLearn(nsIURI* targetURI, nsIURI* sourceURI,
                        PredictorLearnReason reason, nsILoadGroup* loadGroup) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadContext> loadContext;
  OriginAttributes originAttributes;

  if (loadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    loadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      loadContext = do_GetInterface(callbacks);

      if (loadContext) {
        loadContext->GetOriginAttributes(originAttributes);
      }
    }
  }

  return predictor->LearnNative(targetURI, sourceURI, reason, originAttributes);
}

nsresult PredictorLearn(nsIURI* targetURI, nsIURI* sourceURI,
                        PredictorLearnReason reason, dom::Document* document) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsNullOrHttp(targetURI) || !IsNullOrHttp(sourceURI)) {
    return NS_OK;
  }

  nsCOMPtr<nsINetworkPredictor> predictor;
  nsresult rv = EnsureGlobalPredictor(getter_AddRefs(predictor));
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes originAttributes;

  if (document) {
    nsCOMPtr<nsIPrincipal> docPrincipal = document->NodePrincipal();

    if (docPrincipal) {
      originAttributes = docPrincipal->OriginAttributesRef();
    }
  }

  return predictor->LearnNative(targetURI, sourceURI, reason, originAttributes);
}

nsresult PredictorLearnRedirect(nsIURI* targetURI, nsIChannel* channel,
                                const OriginAttributes& originAttributes) {
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

  return predictor->LearnNative(targetURI, sourceURI,
                                nsINetworkPredictor::LEARN_LOAD_REDIRECT,
                                originAttributes);
}

// nsINetworkPredictorVerifier

/**
 * Call through to the child's verifier (only during tests)
 */
NS_IMETHODIMP
Predictor::OnPredictPrefetch(nsIURI* aURI, uint32_t httpStatus) {
  if (IsNeckoChild()) {
    if (mChildVerifier) {
      // Ideally, we'd assert here. But since we're slowly moving towards a
      // world where we have multiple child processes, and only one child
      // process will be likely to have a verifier, we have to play it safer.
      return mChildVerifier->OnPredictPrefetch(aURI, httpStatus);
    }
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(aURI, "aURI must not be null");

  for (auto* cp : dom::ContentParent::AllProcesses(dom::ContentParent::eLive)) {
    PNeckoParent* neckoParent = SingleManagedOrNull(cp->ManagedPNeckoParent());
    if (!neckoParent) {
      continue;
    }
    if (!neckoParent->SendPredOnPredictPrefetch(aURI, httpStatus)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::OnPredictPreconnect(nsIURI* aURI) {
  if (IsNeckoChild()) {
    if (mChildVerifier) {
      // Ideally, we'd assert here. But since we're slowly moving towards a
      // world where we have multiple child processes, and only one child
      // process will be likely to have a verifier, we have to play it safer.
      return mChildVerifier->OnPredictPreconnect(aURI);
    }
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(aURI, "aURI must not be null");

  for (auto* cp : dom::ContentParent::AllProcesses(dom::ContentParent::eLive)) {
    PNeckoParent* neckoParent = SingleManagedOrNull(cp->ManagedPNeckoParent());
    if (!neckoParent) {
      continue;
    }
    if (!neckoParent->SendPredOnPredictPreconnect(aURI)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::OnPredictDNS(nsIURI* aURI) {
  if (IsNeckoChild()) {
    if (mChildVerifier) {
      // Ideally, we'd assert here. But since we're slowly moving towards a
      // world where we have multiple child processes, and only one child
      // process will be likely to have a verifier, we have to play it safer.
      return mChildVerifier->OnPredictDNS(aURI);
    }
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(aURI, "aURI must not be null");

  for (auto* cp : dom::ContentParent::AllProcesses(dom::ContentParent::eLive)) {
    PNeckoParent* neckoParent = SingleManagedOrNull(cp->ManagedPNeckoParent());
    if (!neckoParent) {
      continue;
    }
    if (!neckoParent->SendPredOnPredictDNS(aURI)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  return NS_OK;
}

// Predictor::PrefetchListener
// nsISupports
NS_IMPL_ISUPPORTS(Predictor::PrefetchListener, nsIStreamListener,
                  nsIRequestObserver)

// nsIRequestObserver
NS_IMETHODIMP
Predictor::PrefetchListener::OnStartRequest(nsIRequest* aRequest) {
  mStartTime = TimeStamp::Now();
  return NS_OK;
}

NS_IMETHODIMP
Predictor::PrefetchListener::OnStopRequest(nsIRequest* aRequest,
                                           nsresult aStatusCode) {
  PREDICTOR_LOG(("OnStopRequest this=%p aStatusCode=0x%" PRIX32, this,
                 static_cast<uint32_t>(aStatusCode)));
  NS_ENSURE_ARG(aRequest);
  if (NS_FAILED(aStatusCode)) {
    return aStatusCode;
  }
  Telemetry::AccumulateTimeDelta(Telemetry::PREDICTOR_PREFETCH_TIME,
                                 mStartTime);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (!httpChannel) {
    PREDICTOR_LOG(("    Could not get HTTP Channel!"));
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(httpChannel);
  if (!cachingChannel) {
    PREDICTOR_LOG(("    Could not get caching channel!"));
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;
  uint32_t httpStatus;
  rv = httpChannel->GetResponseStatus(&httpStatus);
  if (NS_SUCCEEDED(rv) && httpStatus == 200) {
    rv = cachingChannel->ForceCacheEntryValidFor(
        StaticPrefs::network_predictor_prefetch_force_valid_for());
    PREDICTOR_LOG(("    forcing entry valid for %d seconds rv=%" PRIX32,
                   StaticPrefs::network_predictor_prefetch_force_valid_for(),
                   static_cast<uint32_t>(rv)));
  } else {
    rv = cachingChannel->ForceCacheEntryValidFor(0);
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_PREDICTOR_PREFETCH_USE_STATUS::Not200);
    PREDICTOR_LOG(("    removing any forced validity rv=%" PRIX32,
                   static_cast<uint32_t>(rv)));
  }

  nsAutoCString reqName;
  rv = aRequest->GetName(reqName);
  if (NS_FAILED(rv)) {
    reqName.AssignLiteral("<unknown>");
  }

  PREDICTOR_LOG(("    request %s status %u", reqName.get(), httpStatus));

  if (mVerifier) {
    mVerifier->OnPredictPrefetch(mURI, httpStatus);
  }

  return rv;
}

// nsIStreamListener
NS_IMETHODIMP
Predictor::PrefetchListener::OnDataAvailable(nsIRequest* aRequest,
                                             nsIInputStream* aInputStream,
                                             uint64_t aOffset,
                                             const uint32_t aCount) {
  uint32_t result;
  return aInputStream->ReadSegments(NS_DiscardSegment, nullptr, aCount,
                                    &result);
}

// Miscellaneous Predictor

void Predictor::UpdateCacheability(nsIURI* sourceURI, nsIURI* targetURI,
                                   uint32_t httpStatus,
                                   nsHttpRequestHead& requestHead,
                                   nsHttpResponseHead* responseHead,
                                   nsILoadContextInfo* lci, bool isTracking) {
  MOZ_ASSERT(NS_IsMainThread());

  if (lci && lci->IsPrivate()) {
    PREDICTOR_LOG(("Predictor::UpdateCacheability in PB mode - ignoring"));
    return;
  }

  if (!sourceURI || !targetURI) {
    PREDICTOR_LOG(
        ("Predictor::UpdateCacheability missing source or target uri"));
    return;
  }

  if (!IsNullOrHttp(sourceURI) || !IsNullOrHttp(targetURI)) {
    PREDICTOR_LOG(("Predictor::UpdateCacheability non-http(s) uri"));
    return;
  }

  RefPtr<Predictor> self = sSelf;
  if (self) {
    nsAutoCString method;
    requestHead.Method(method);

    nsAutoCString vary;
    Unused << responseHead->GetHeader(nsHttp::Vary, vary);

    nsAutoCString cacheControlHeader;
    Unused << responseHead->GetHeader(nsHttp::Cache_Control,
                                      cacheControlHeader);
    CacheControlParser cacheControl(cacheControlHeader);

    self->UpdateCacheabilityInternal(sourceURI, targetURI, httpStatus, method,
                                     *lci->OriginAttributesPtr(), isTracking,
                                     !vary.IsEmpty(), cacheControl.NoStore());
  }
}

void Predictor::UpdateCacheabilityInternal(
    nsIURI* sourceURI, nsIURI* targetURI, uint32_t httpStatus,
    const nsCString& method, const OriginAttributes& originAttributes,
    bool isTracking, bool couldVary, bool isNoStore) {
  PREDICTOR_LOG(("Predictor::UpdateCacheability httpStatus=%u", httpStatus));

  nsresult rv;

  if (!mInitialized) {
    PREDICTOR_LOG(("    not initialized"));
    return;
  }

  if (!StaticPrefs::network_predictor_enabled()) {
    PREDICTOR_LOG(("    not enabled"));
    return;
  }

  nsCOMPtr<nsICacheStorage> cacheDiskStorage;

  RefPtr<LoadContextInfo> lci = new LoadContextInfo(false, originAttributes);

  rv = mCacheStorageService->DiskCacheStorage(lci,
                                              getter_AddRefs(cacheDiskStorage));
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(("    cannot get disk cache storage"));
    return;
  }

  uint32_t openFlags = nsICacheStorage::OPEN_READONLY |
                       nsICacheStorage::OPEN_SECRETLY |
                       nsICacheStorage::CHECK_MULTITHREADED;
  RefPtr<Predictor::CacheabilityAction> action =
      new Predictor::CacheabilityAction(targetURI, httpStatus, method,
                                        isTracking, couldVary, isNoStore, this);
  nsAutoCString uri;
  targetURI->GetAsciiSpec(uri);
  PREDICTOR_LOG(("    uri=%s action=%p", uri.get(), action.get()));
  cacheDiskStorage->AsyncOpenURI(sourceURI, ""_ns, openFlags, action);
}

NS_IMPL_ISUPPORTS(Predictor::CacheabilityAction, nsICacheEntryOpenCallback,
                  nsICacheEntryMetaDataVisitor);

NS_IMETHODIMP
Predictor::CacheabilityAction::OnCacheEntryCheck(nsICacheEntry* entry,
                                                 uint32_t* result) {
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

namespace {
enum PrefetchDecisionReason {
  PREFETCHABLE,
  STATUS_NOT_200,
  METHOD_NOT_GET,
  URL_HAS_QUERY_STRING,
  RESOURCE_IS_TRACKING,
  RESOURCE_COULD_VARY,
  RESOURCE_IS_NO_STORE
};
}

NS_IMETHODIMP
Predictor::CacheabilityAction::OnCacheEntryAvailable(nsICacheEntry* entry,
                                                     bool isNew,
                                                     nsresult result) {
  MOZ_ASSERT(NS_IsMainThread());
  // This is being opened read-only, so isNew should always be false
  MOZ_ASSERT(!isNew);

  PREDICTOR_LOG(("CacheabilityAction::OnCacheEntryAvailable this=%p", this));
  if (NS_FAILED(result)) {
    // Nothing to do
    PREDICTOR_LOG(("    nothing to do result=%" PRIX32 " isNew=%d",
                   static_cast<uint32_t>(result), isNew));
    return NS_OK;
  }

  nsCString strTargetURI;
  nsresult rv = mTargetURI->GetAsciiSpec(strTargetURI);
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    GetAsciiSpec returned %" PRIx32, static_cast<uint32_t>(rv)));
    return NS_OK;
  }

  rv = entry->VisitMetaData(this);
  if (NS_FAILED(rv)) {
    PREDICTOR_LOG(
        ("    VisitMetaData returned %" PRIx32, static_cast<uint32_t>(rv)));
    return NS_OK;
  }

  nsTArray<nsCString> keysToCheck = std::move(mKeysToCheck),
                      valuesToCheck = std::move(mValuesToCheck);

  bool hasQueryString = false;
  nsAutoCString query;
  if (NS_SUCCEEDED(mTargetURI->GetQuery(query)) && !query.IsEmpty()) {
    hasQueryString = true;
  }

  MOZ_ASSERT(keysToCheck.Length() == valuesToCheck.Length());
  for (size_t i = 0; i < keysToCheck.Length(); ++i) {
    const char* key = keysToCheck[i].BeginReading();
    const char* value = valuesToCheck[i].BeginReading();
    nsCString uri;
    uint32_t hitCount, lastHit, flags;

    if (!mPredictor->ParseMetaDataEntry(key, value, uri, hitCount, lastHit,
                                        flags)) {
      PREDICTOR_LOG(("    failed to parse key=%s value=%s", key, value));
      continue;
    }

    if (strTargetURI.Equals(uri)) {
      bool prefetchable = true;
      PrefetchDecisionReason reason = PREFETCHABLE;

      if (mHttpStatus != 200) {
        prefetchable = false;
        reason = STATUS_NOT_200;
      } else if (!mMethod.EqualsLiteral("GET")) {
        prefetchable = false;
        reason = METHOD_NOT_GET;
      } else if (hasQueryString) {
        prefetchable = false;
        reason = URL_HAS_QUERY_STRING;
      } else if (mIsTracking) {
        prefetchable = false;
        reason = RESOURCE_IS_TRACKING;
      } else if (mCouldVary) {
        prefetchable = false;
        reason = RESOURCE_COULD_VARY;
      } else if (mIsNoStore) {
        // We don't set prefetchable = false yet, because we just want to know
        // what kind of effect this would have on prefetching.
        reason = RESOURCE_IS_NO_STORE;
      }

      Telemetry::Accumulate(Telemetry::PREDICTOR_PREFETCH_DECISION_REASON,
                            reason);

      if (prefetchable) {
        PREDICTOR_LOG(("    marking %s cacheable", key));
        flags |= FLAG_PREFETCHABLE;
      } else {
        PREDICTOR_LOG(("    marking %s uncacheable", key));
        flags &= ~FLAG_PREFETCHABLE;
      }
      nsCString newValue;
      MakeMetadataEntry(hitCount, lastHit, flags, newValue);
      entry->SetMetaDataElement(key, newValue.BeginReading());
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Predictor::CacheabilityAction::OnMetaDataElement(const char* asciiKey,
                                                 const char* asciiValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsURIMetadataElement(asciiKey)) {
    return NS_OK;
  }

  nsCString key, value;
  key.AssignASCII(asciiKey);
  value.AssignASCII(asciiValue);
  mKeysToCheck.AppendElement(key);
  mValuesToCheck.AppendElement(value);

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
