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
#include "nsIURI.h"
#include "nsIWebProgressListener.h"
#include "nsIPrincipal.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsTHashMap.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

// Global map: browserId -> BounceTrackingState
static StaticAutoPtr<nsTHashMap<uint64_t, RefPtr<BounceTrackingState>>>
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
}

// static
already_AddRefed<BounceTrackingState> BounceTrackingState::GetOrCreate(
    dom::BrowsingContextWebProgress* aWebProgress) {
  MOZ_ASSERT(aWebProgress);

  if (!ShouldCreateBounceTrackingStateForWebProgress(aWebProgress)) {
    return nullptr;
  }

  // Create BounceTrackingState instance and populate the global
  // BounceTrackingState map.
  if (!sBounceTrackingStates) {
    sBounceTrackingStates =
        new nsTHashMap<nsUint64HashKey, RefPtr<BounceTrackingState>>();
    ClearOnShutdown(&sBounceTrackingStates);
  }

  if (!sStorageObserver) {
    sStorageObserver = new BounceTrackingStorageObserver();
    ClearOnShutdown(&sStorageObserver);

    DebugOnly<nsresult> rv = sStorageObserver->Init();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to init storage observer");
  }

  dom::BrowsingContext* browsingContext = aWebProgress->GetBrowsingContext();
  if (!browsingContext) {
    return nullptr;
  }
  uint64_t browserId = browsingContext->BrowserId();
  bool createdNew;
  RefPtr<BounceTrackingState> bounceTrackingState =
      do_AddRef(sBounceTrackingStates->LookupOrInsertWith(browserId, [&] {
        createdNew = true;
        return do_AddRef(new BounceTrackingState());
      }));

  if (createdNew) {
    nsresult rv = bounceTrackingState->Init(aWebProgress);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  return bounceTrackingState.forget();
};

void BounceTrackingState::ResetAll() {
  if (!sBounceTrackingStates) {
    return;
  }
  for (const RefPtr<BounceTrackingState>& bounceTrackingState :
       sBounceTrackingStates->Values()) {
    if (bounceTrackingState->mClientBounceDetectionTimeout) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: mClientBounceDetectionTimeout->Cancel()", __FUNCTION__));
      bounceTrackingState->mClientBounceDetectionTimeout->Cancel();
      bounceTrackingState->mClientBounceDetectionTimeout = nullptr;
    }
    bounceTrackingState->ResetBounceTrackingRecord();
  }
}

nsresult BounceTrackingState::Init(
    dom::BrowsingContextWebProgress* aWebProgress) {
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

  // Add a listener for window load. See BounceTrackingState::OnStateChange for
  // the listener code.
  nsresult rv = aWebProgress->AddProgressListener(
      this, nsIWebProgress::NOTIFY_STATE_WINDOW);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void BounceTrackingState::ResetBounceTrackingRecord() {
  mBounceTrackingRecord = nullptr;
}

BounceTrackingRecord* BounceTrackingState::GetBounceTrackingRecord() {
  return mBounceTrackingRecord;
}

nsCString BounceTrackingState::Describe() {
  return nsPrintfCString(
      "{ mBounceTrackingRecord: %s }",
      mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get() : "null");
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
  for (const RefPtr<BounceTrackingState>& state :
       sBounceTrackingStates->Values()) {
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

nsresult BounceTrackingState::OnDocumentStartRequest(nsIChannel* aChannel) {
  NS_ENSURE_ARG_POINTER(aChannel);
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug, ("%s", __FUNCTION__));

  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv = aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Collect uri list including any redirects.
  nsTArray<nsCString> siteList;

  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectHistoryEntry :
       loadInfo->RedirectChain()) {
    nsCOMPtr<nsIPrincipal> principal;
    rv = redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Filter out non-content principals.
    if (!principal->GetIsContentPrincipal()) {
      continue;
    }

    nsAutoCString baseDomain;
    rv = principal->GetBaseDomain(baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    siteList.AppendElement(baseDomain);
  }

  // Add site via the current URI which is the end of the chain.
  nsCOMPtr<nsIURI> channelURI;
  rv = aChannel->GetURI(getter_AddRefs(channelURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString siteHost;
  rv = tldService->GetSchemelessSite(channelURI, siteHost);

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retrieve site for final channel URI.");
  }

  siteList.AppendElement(siteHost);

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
  // interested in content principals. Other principal types are not considered
  // to be trackers.
  if (!aTriggeringPrincipal->GetIsContentPrincipal()) {
    siteHost = "";
  }

  // obtain site
  nsresult rv = aTriggeringPrincipal->GetBaseDomain(siteHost);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    siteHost = "";
  }

  // If navigable’s bounce tracking record is null: Set navigable’s bounce
  // tracking record to a new bounce tracking record with initial host set to
  // initialHost.
  if (!mBounceTrackingRecord) {
    mBounceTrackingRecord = new BounceTrackingRecord();
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
    rv = mBounceTrackingProtection->RecordStatefulBounces(this);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(!mBounceTrackingRecord);
    mBounceTrackingRecord = new BounceTrackingRecord();
    mBounceTrackingRecord->SetInitialHost(siteHost);

    return NS_OK;
  }

  // There is no transient user activation. Add host as a bounce candidate.
  mBounceTrackingRecord->AddBounceHost(siteHost);

  return NS_OK;
}

// Private

nsresult BounceTrackingState::OnResponseReceived(
    const nsTArray<nsCString>& aSiteList) {
  NS_ENSURE_TRUE(mBounceTrackingRecord, NS_ERROR_FAILURE);

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

  // Check if there is still an active timeout. This shouldn't happen since
  // OnStartNavigation already cancels it.
  if (NS_WARN_IF(mClientBounceDetectionTimeout)) {
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
        bounceTrackingState->mBounceTrackingProtection->RecordStatefulBounces(
            bounceTrackingState);

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

  // Assert: navigable’s bounce tracking record is not null.
  NS_ENSURE_TRUE(mBounceTrackingRecord, NS_ERROR_FAILURE);

  // Logging
  if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
    nsAutoCString origin;
    nsresult rv = aDocumentPrincipal->GetOrigin(origin);
    if (NS_FAILED(rv)) {
      origin = "err";
    }
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: origin: %s, mBounceTrackingRecord: %s", __FUNCTION__,
             origin.get(),
             mBounceTrackingRecord ? mBounceTrackingRecord->Describe().get()
                                   : "null"));
  }

  nsAutoCString siteHost;
  if (!aDocumentPrincipal->GetIsContentPrincipal()) {
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

}  // namespace mozilla
