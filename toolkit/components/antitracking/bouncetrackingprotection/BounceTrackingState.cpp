/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingProtection.h"
#include "BounceTrackingState.h"
#include "BounceTrackingRecord.h"

#include "BounceTrackingStorageObserver.h"
#include "ErrorList.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextWebProgress.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIBrowser.h"
#include "nsIChannel.h"
#include "nsIEffectiveTLDService.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsICookieManager.h"
#include "nsICookieService.h"
#include "nsIURI.h"
#include "nsIWebProgressListener.h"
#include "nsIPrincipal.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsTHashMap.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/WindowContext.h"

namespace mozilla {

// Global map: browserId -> BounceTrackingState
static StaticAutoPtr<nsTHashMap<uint64_t, WeakPtr<BounceTrackingState>>>
    sBounceTrackingStates;

static StaticRefPtr<BounceTrackingStorageObserver> sStorageObserver;

NS_IMPL_ISUPPORTS(BounceTrackingState, nsIWebProgressListener,
                  nsISupportsWeakReference);

BounceTrackingState::BounceTrackingState() {
  MOZ_ASSERT(StaticPrefs::privacy_bounceTrackingProtection_enabled_AtStartup());
  mBounceTrackingProtection = BounceTrackingProtection::GetSingleton();
};

BounceTrackingState::~BounceTrackingState() {
  if (sBounceTrackingStates) {
    sBounceTrackingStates->Remove(mBrowserId);
  }
  if (mClientBounceDetectionTimeout) {
    mClientBounceDetectionTimeout->Cancel();
    mClientBounceDetectionTimeout = nullptr;
  }
}

// static
already_AddRefed<BounceTrackingState> BounceTrackingState::GetOrCreate(
    dom::BrowsingContextWebProgress* aWebProgress, nsresult& aRv) {
  aRv = NS_OK;

  if (!aWebProgress) {
    aRv = NS_ERROR_INVALID_ARG;
    return nullptr;
  }

  if (!ShouldCreateBounceTrackingStateForWebProgress(aWebProgress)) {
    return nullptr;
  }

  dom::BrowsingContext* browsingContext = aWebProgress->GetBrowsingContext();
  if (!browsingContext) {
    return nullptr;
  }
  uint64_t browserId = browsingContext->BrowserId();

  // Check if there is already a BounceTrackingState for the given browser.
  if (sBounceTrackingStates) {
    WeakPtr<BounceTrackingState> existingBTS =
        sBounceTrackingStates->Get(browserId);
    if (existingBTS) {
      // Return a strong reference.
      return do_AddRef(existingBTS.get());
    }
  }

  // Create a new BounceTracking state, initialize it and insert it into the
  // map, then return it to the caller.
  RefPtr<BounceTrackingState> newBTS = new BounceTrackingState();

  aRv = newBTS->Init(aWebProgress);
  if (NS_FAILED(aRv)) {
    NS_WARNING("Failed to initialize BounceTrackingState.");
    return nullptr;
  }

  // Now that we know that we'll keep this BounceTrackingState lazily initialize
  // the global map
  if (!sBounceTrackingStates) {
    sBounceTrackingStates =
        new nsTHashMap<nsUint64HashKey, WeakPtr<BounceTrackingState>>();
    ClearOnShutdown(&sBounceTrackingStates);
  }
  sBounceTrackingStates->InsertOrUpdate(browserId, newBTS);

  // And the storage observer.
  if (!sStorageObserver) {
    sStorageObserver = new BounceTrackingStorageObserver();
    ClearOnShutdown(&sStorageObserver);

    aRv = sStorageObserver->Init();
    NS_ENSURE_SUCCESS(aRv, nullptr);
  }

  return newBTS.forget();
};

// static
void BounceTrackingState::ResetAll() { Reset(nullptr, nullptr); }

// static
void BounceTrackingState::ResetAllForOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  Reset(&aOriginAttributes, nullptr);
}

// static
void BounceTrackingState::ResetAllForOriginAttributesPattern(
    const OriginAttributesPattern& aPattern) {
  Reset(nullptr, &aPattern);
}

nsresult BounceTrackingState::Init(
    dom::BrowsingContextWebProgress* aWebProgress) {
  MOZ_ASSERT(!mIsInitialized,
             "BounceTrackingState must not be initialized twice.");
  mIsInitialized = true;

  NS_ENSURE_ARG_POINTER(aWebProgress);
  NS_ENSURE_TRUE(
      StaticPrefs::privacy_bounceTrackingProtection_enabled_AtStartup(),
      NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mBounceTrackingProtection, NS_ERROR_FAILURE);

  // Store the browser ID so we can get the associated BC later without having
  // to hold a reference to aWebProgress.
  dom::BrowsingContext* browsingContext = aWebProgress->GetBrowsingContext();
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_FAILURE);
  mBrowserId = browsingContext->BrowserId();
  // Create a copy of the BC's OriginAttributes so we can use it later without
  // having to hold a reference to the BC.
  mOriginAttributes = browsingContext->OriginAttributesRef();
  MOZ_ASSERT(mOriginAttributes.mPartitionKey.IsEmpty(),
             "Top level BCs mus not have a partition key.");

  // Add a listener for window load. See BounceTrackingState::OnStateChange for
  // the listener code.
  nsresult rv = aWebProgress->AddProgressListener(
      this, nsIWebProgress::NOTIFY_STATE_WINDOW);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void BounceTrackingState::ResetBounceTrackingRecord() {
  mBounceTrackingRecord = Nothing();
}

const Maybe<BounceTrackingRecord>&
BounceTrackingState::GetBounceTrackingRecord() {
  return mBounceTrackingRecord;
}

nsCString BounceTrackingState::Describe() {
  nsAutoCString oaSuffix;
  OriginAttributesRef().CreateSuffix(oaSuffix);

  return nsPrintfCString(
      "{ mBounceTrackingRecord: %s, mOriginAttributes: %s, mBrowserId: %" PRIu64
      " }",
      mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get() : "null",
      oaSuffix.get(), mBrowserId);
}

// static
void BounceTrackingState::Reset(const OriginAttributes* aOriginAttributes,
                                const OriginAttributesPattern* aPattern) {
  if (aOriginAttributes || aPattern) {
    MOZ_ASSERT((aOriginAttributes != nullptr) != (aPattern != nullptr),
               "Must not pass both aOriginAttributes and aPattern.");
  }

  if (!sBounceTrackingStates) {
    return;
  }
  for (const WeakPtr<BounceTrackingState>& btsWeak :
       sBounceTrackingStates->Values()) {
    if (!btsWeak) {
      continue;
    }
    RefPtr<BounceTrackingState> bounceTrackingState(btsWeak);

    if ((aOriginAttributes &&
         *aOriginAttributes != bounceTrackingState->OriginAttributesRef()) ||
        (aPattern &&
         !aPattern->Matches(bounceTrackingState->OriginAttributesRef()))) {
      continue;
    }
    if (bounceTrackingState->mClientBounceDetectionTimeout) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: mClientBounceDetectionTimeout->Cancel()", __FUNCTION__));
      bounceTrackingState->mClientBounceDetectionTimeout->Cancel();
      bounceTrackingState->mClientBounceDetectionTimeout = nullptr;
    }
    bounceTrackingState->ResetBounceTrackingRecord();
  }
}

// static
bool BounceTrackingState::ShouldCreateBounceTrackingStateForWebProgress(
    dom::BrowsingContextWebProgress* aWebProgress) {
  NS_ENSURE_TRUE(aWebProgress, false);

  // Feature is globally disabled.
  if (!StaticPrefs::privacy_bounceTrackingProtection_enabled_AtStartup()) {
    return false;
  }

  // Only keep track of top level content browsing contexts.
  dom::BrowsingContext* browsingContext = aWebProgress->GetBrowsingContext();
  if (!browsingContext || !browsingContext->IsTopContent()) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("%s: Skip non top-content.", __FUNCTION__));
    return false;
  }

  bool isPrivate = browsingContext->UsePrivateBrowsing();
  uint32_t cookieBehavior = nsICookieManager::GetCookieBehavior(isPrivate);
  if (cookieBehavior == nsICookieService::BEHAVIOR_ACCEPT ||
      cookieBehavior == nsICookieService::BEHAVIOR_REJECT) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
            ("%s: Skip on cookie behavior %i", __FUNCTION__, cookieBehavior));
    return false;
  }

  return true;
}

// static
bool BounceTrackingState::ShouldTrackPrincipal(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  // Only track content principals.
  if (!aPrincipal->GetIsContentPrincipal()) {
    return false;
  }

  // Skip non http schemes.
  if (!aPrincipal->SchemeIs("http") && !aPrincipal->SchemeIs("https")) {
    return false;
  }

  // Skip partitioned principals.
  if (!aPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty()) {
    return false;
  }

  return true;
}

// static
nsresult BounceTrackingState::HasBounceTrackingStateForSite(
    const nsACString& aSiteHost, bool& aResult) {
  aResult = false;
  NS_ENSURE_TRUE(aSiteHost.Length(), NS_ERROR_FAILURE);

  if (!sBounceTrackingStates) {
    return NS_OK;
  }

  // Iterate over all browsing contexts which have a bounce tracking state. Use
  // the content principal base domain field to determine whether a BC has an
  // active site that matches aSiteHost.
  for (const WeakPtr<BounceTrackingState>& btsWeak :
       sBounceTrackingStates->Values()) {
    if (!btsWeak) {
      continue;
    }
    RefPtr<BounceTrackingState> state(btsWeak);

    RefPtr<dom::BrowsingContext> browsingContext =
        state->CurrentBrowsingContext();

    if (!browsingContext || browsingContext->IsDiscarded() ||
        browsingContext->IsInBFCache()) {
      continue;
    }

    RefPtr<dom::Element> embedderElement =
        browsingContext->GetEmbedderElement();
    if (!embedderElement) {
      continue;
    }

    nsCOMPtr<nsIBrowser> browser = embedderElement->AsBrowser();
    if (!browser) {
      continue;
    }

    nsCOMPtr<nsIPrincipal> contentPrincipal;
    nsresult rv =
        browser->GetContentPrincipal(getter_AddRefs(contentPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsAutoCString baseDomain;
    rv = contentPrincipal->GetBaseDomain(baseDomain);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    if (aSiteHost.Equals(baseDomain)) {
      aResult = true;
      return NS_OK;
    }
  }

  return NS_OK;
}

already_AddRefed<dom::BrowsingContext>
BounceTrackingState::CurrentBrowsingContext() {
  MOZ_ASSERT(mBrowserId != 0);
  return dom::BrowsingContext::GetCurrentTopByBrowserId(mBrowserId);
}

const OriginAttributes& BounceTrackingState::OriginAttributesRef() {
  return mOriginAttributes;
}

nsresult BounceTrackingState::OnDocumentStartRequest(nsIChannel* aChannel) {
  NS_ENSURE_ARG_POINTER(aChannel);
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug, ("%s", __FUNCTION__));

  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv = aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Used to keep track of whether we added entries to the site list that are
  // not "null".
  bool siteListIsEmpty = true;

  // Collect uri list including any redirects.
  nsTArray<nsCString> siteList;

  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectHistoryEntry :
       loadInfo->RedirectChain()) {
    nsCOMPtr<nsIPrincipal> principal;
    rv = redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!BounceTrackingState::ShouldTrackPrincipal(principal)) {
      siteList.AppendElement("null"_ns);
      continue;
    }

    nsAutoCString baseDomain;
    rv = principal->GetBaseDomain(baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_WARN_IF(baseDomain.IsEmpty())) {
      siteList.AppendElement("null");
    } else {
      siteList.AppendElement(baseDomain);
      siteListIsEmpty = false;
    }
  }

  // Add site via the current URI which is the end of the chain.
  nsCOMPtr<nsIURI> channelURI;
  rv = aChannel->GetURI(getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, rv);

  if (channelURI->SchemeIs("http") || channelURI->SchemeIs("https")) {
    nsCOMPtr<nsIEffectiveTLDService> tldService =
        do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString siteHost;
    rv = tldService->GetSchemelessSite(channelURI, siteHost);

    // Skip URIs where we can't get a site host.
    if (NS_FAILED(rv)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Failed to get site host from channelURI: %s", __FUNCTION__,
               channelURI->GetSpecOrDefault().get()));
      siteList.AppendElement("null"_ns);
    } else {
      MOZ_ASSERT(!siteHost.IsEmpty(), "siteHost should not be empty.");
      siteList.AppendElement(siteHost);
      siteListIsEmpty = false;
    }
  }

  // Do not record empty site lists. This can happen if none of the principals
  // are suitable for tracking. It includes when OnDocumentStartRequest is
  // called for the initial about:blank.
  if (siteListIsEmpty) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: skip empty site list.", __FUNCTION__));
    return NS_OK;
  }

  return OnResponseReceived(siteList);
}

// nsIWebProgressListener

NS_IMETHODIMP
BounceTrackingState::OnStateChange(nsIWebProgress* aWebProgress,
                                   nsIRequest* aRequest, uint32_t aStateFlags,
                                   nsresult aStatus) {
  NS_ENSURE_ARG_POINTER(aWebProgress);
  NS_ENSURE_ARG_POINTER(aRequest);

  bool isTopLevel = false;
  nsresult rv = aWebProgress->GetIsTopLevel(&isTopLevel);
  NS_ENSURE_SUCCESS(rv, rv);

  // Filter for top level loads.
  if (!isTopLevel) {
    return NS_OK;
  }

  // Filter for window loads.
  if (!(aStateFlags & nsIWebProgressListener::STATE_STOP) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_WINDOW)) {
    return NS_OK;
  }

  // Get the document principal via the current window global.
  dom::BrowsingContext* browsingContext = aWebProgress->GetBrowsingContext();
  NS_ENSURE_TRUE(browsingContext, NS_ERROR_FAILURE);

  dom::WindowGlobalParent* windowGlobalParent =
      browsingContext->Canonical()->GetCurrentWindowGlobal();
  NS_ENSURE_TRUE(windowGlobalParent, NS_ERROR_FAILURE);

  return OnDocumentLoaded(windowGlobalParent->DocumentPrincipal());
}

NS_IMETHODIMP
BounceTrackingState::OnProgressChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest,
                                      int32_t aCurSelfProgress,
                                      int32_t aMaxSelfProgress,
                                      int32_t aCurTotalProgress,
                                      int32_t aMaxTotalProgress) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingState::OnLocationChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest, nsIURI* aLocation,
                                      uint32_t aFlags) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingState::OnStatusChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, nsresult aStatus,
                                    const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingState::OnSecurityChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest, uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingState::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

nsresult BounceTrackingState::OnStartNavigation(
    nsIPrincipal* aTriggeringPrincipal,
    const bool aHasValidUserGestureActivation) {
  NS_ENSURE_ARG_POINTER(aTriggeringPrincipal);

  // Logging
  if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
    nsAutoCString origin;
    nsresult rv = aTriggeringPrincipal->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      origin = "err";
    }
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: origin: %s, mBounceTrackingRecord: %s", __FUNCTION__,
             origin.get(),
             mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get()
                                   : "null"));
  }

  // Remove any queued global tasks to record stateful bounces for bounce
  // tracking from the networking task source.
  if (mClientBounceDetectionTimeout) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: mClientBounceDetectionTimeout->Cancel()", __FUNCTION__));
    mClientBounceDetectionTimeout->Cancel();
    mClientBounceDetectionTimeout = nullptr;
  }

  // Obtain the (schemeless) site to keep track of bounces.
  nsAutoCString siteHost;

  // If origin is an opaque origin, set initialHost to empty host. Strictly
  // speaking we only need to check IsNullPrincipal, but we're generally only
  // interested in content principals with http/s scheme. Other principal types
  // or schemes are not considered to be trackers.
  if (!BounceTrackingState::ShouldTrackPrincipal(aTriggeringPrincipal)) {
    siteHost = "";
  } else {
    // obtain site
    nsresult rv = aTriggeringPrincipal->GetBaseDomain(siteHost);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      siteHost = "";
    }
  }

  // If navigable’s bounce tracking record is null: Set navigable’s bounce
  // tracking record to a new bounce tracking record with initial host set to
  // initialHost.
  if (!mBounceTrackingRecord) {
    mBounceTrackingRecord = Some(BounceTrackingRecord());
    mBounceTrackingRecord->SetInitialHost(siteHost);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: new BounceTrackingRecord(): %s", __FUNCTION__,
             mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get()
                                   : "null"));

    return NS_OK;
  }

  // If sourceSnapshotParams’s has transient activation is true: The user
  // activation ends the extended navigation. Process the bounce candidates.
  // Also treat system principal navigation as having user interaction
  bool hasUserActivation = aHasValidUserGestureActivation ||
                           aTriggeringPrincipal->IsSystemPrincipal();

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: site: %s, hasUserActivation? %d", __FUNCTION__, siteHost.get(),
           hasUserActivation));
  if (hasUserActivation) {
    nsresult rv = mBounceTrackingProtection->RecordStatefulBounces(this);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(!mBounceTrackingRecord);
    mBounceTrackingRecord = Some(BounceTrackingRecord());
    mBounceTrackingRecord->SetInitialHost(siteHost);

    return NS_OK;
  }

  // There is no transient user activation. Add host as a bounce candidate.
  if (siteHost.IsEmpty()) {
    mBounceTrackingRecord->AddBounceHost("null"_ns);
  } else {
    mBounceTrackingRecord->AddBounceHost(siteHost);
  }

  return NS_OK;
}

// Private

nsresult BounceTrackingState::OnResponseReceived(
    const nsTArray<nsCString>& aSiteList) {
#ifdef DEBUG
  MOZ_ASSERT(!aSiteList.IsEmpty(), "siteList should not be empty.");
  for (const nsCString& site : aSiteList) {
    MOZ_ASSERT(!site.IsEmpty(), "site should not be an empty string.");
  }
#endif

  // Logging
  if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
    nsAutoCString siteListStr;

    for (const nsACString& site : aSiteList) {
      siteListStr.Append(site);
      siteListStr.AppendLiteral(", ");
    }

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: #%zu siteList: %s", __FUNCTION__, siteListStr.Length(),
             siteListStr.get()));
  }

  // Record should exist by now. It gets created in OnStartNavigation.
  // TODO: Bug 1894936
  if (!mBounceTrackingRecord) {
    return NS_ERROR_FAILURE;
  }

  // Check if there is still an active timeout. This shouldn't happen since
  // OnStartNavigation already cancels it.
  // TODO: Bug 1894936
  if (mClientBounceDetectionTimeout) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: mClientBounceDetectionTimeout->Cancel()", __FUNCTION__));
    mClientBounceDetectionTimeout->Cancel();
    mClientBounceDetectionTimeout = nullptr;
  }

  // Run steps after a timeout: queue a global task on the networking task
  // source with global to record stateful bounces for bounce.
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: Scheduling mClientBounceDetectionTimeout", __FUNCTION__));

  // Use a weak reference to this to avoid keeping the object alive if the tab
  // is closed during the timeout.
  WeakPtr<BounceTrackingState> thisWeak = this;
  nsresult rv = NS_NewTimerWithCallback(
      getter_AddRefs(mClientBounceDetectionTimeout),
      [thisWeak](auto) {
        if (!thisWeak) {
          MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                  ("%s: !thisWeak", __FUNCTION__));
          return;
        }
        MOZ_LOG(
            gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Calling RecordStatefulBounces after timeout.", __FUNCTION__));

        BounceTrackingState* bounceTrackingState = thisWeak;
        DebugOnly<nsresult> rv =
            bounceTrackingState->mBounceTrackingProtection
                ->RecordStatefulBounces(bounceTrackingState);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "Running RecordStatefulBounces after a timeout failed.");

        bounceTrackingState->mClientBounceDetectionTimeout = nullptr;
      },
      StaticPrefs::
          privacy_bounceTrackingProtection_clientBounceDetectionTimerPeriodMS(),
      nsITimer::TYPE_ONE_SHOT, "mClientBounceDetectionTimeout");
  NS_ENSURE_SUCCESS(rv, rv);

  // For each URL in URLs: Insert host to the navigable’s bounce tracking
  // record's bounce set.
  for (const nsACString& site : aSiteList) {
    mBounceTrackingRecord->AddBounceHost(site);
  }

  return NS_OK;
}

nsresult BounceTrackingState::OnDocumentLoaded(
    nsIPrincipal* aDocumentPrincipal) {
  NS_ENSURE_ARG_POINTER(aDocumentPrincipal);

  // Logging
  if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
    nsAutoCString origin;
    nsresult rv = aDocumentPrincipal->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      origin = "err";
    }
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: origin: %s, this: %s", __FUNCTION__, origin.get(),
             Describe().get()));
  }

  // Assert: navigable’s bounce tracking record is not null.
  // TODO: Bug 1894936
  if (!mBounceTrackingRecord) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString siteHost;
  if (!BounceTrackingState::ShouldTrackPrincipal(aDocumentPrincipal)) {
    siteHost = "";
  } else {
    nsresult rv = aDocumentPrincipal->GetBaseDomain(siteHost);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the navigable’s bounce tracking record's final host to the host of
  // finalSite.
  mBounceTrackingRecord->SetFinalHost(siteHost);

  return NS_OK;
}

nsresult BounceTrackingState::OnCookieWrite(const nsACString& aSiteHost) {
  NS_ENSURE_TRUE(!aSiteHost.IsEmpty(), NS_ERROR_FAILURE);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Verbose,
          ("%s: OnCookieWrite: %s.", __FUNCTION__,
           PromiseFlatCString(aSiteHost).get()));

  if (!mBounceTrackingRecord) {
    return NS_OK;
  }

  mBounceTrackingRecord->AddStorageAccessHost(aSiteHost);
  return NS_OK;
}

nsresult BounceTrackingState::OnStorageAccess(nsIPrincipal* aPrincipal) {
  NS_ENSURE_ARG_POINTER(aPrincipal);
  // The caller should already filter out principals for us.
  MOZ_ASSERT(BounceTrackingState::ShouldTrackPrincipal(aPrincipal));

  if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
    nsAutoCString origin;
    nsresult rv = aPrincipal->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      origin = "err";
    }
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: origin: %s, mBounceTrackingRecord: %s", __FUNCTION__,
             origin.get(),
             mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get()
                                   : "null"));
  }

  if (!mBounceTrackingRecord) {
    return NS_OK;
  }

  nsAutoCString siteHost;
  nsresult rv = aPrincipal->GetBaseDomain(siteHost);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!siteHost.IsEmpty(), NS_ERROR_FAILURE);

  mBounceTrackingRecord->AddStorageAccessHost(siteHost);

  return NS_OK;
}

}  // namespace mozilla
