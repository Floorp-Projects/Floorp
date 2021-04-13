/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelClassifierService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/UrlClassifierCommon.h"

#include "UrlClassifierFeatureCryptominingProtection.h"
#include "UrlClassifierFeatureFingerprintingProtection.h"
#include "UrlClassifierFeatureSocialTrackingProtection.h"
#include "UrlClassifierFeatureTrackingProtection.h"

namespace mozilla {
namespace net {

static StaticRefPtr<ChannelClassifierService> gChannelClassifierService;

NS_IMPL_ISUPPORTS(UrlClassifierBlockedChannel, nsIUrlClassifierBlockedChannel)

UrlClassifierBlockedChannel::UrlClassifierBlockedChannel(nsIChannel* aChannel)
    : mChannel(aChannel),
      mDecision(ChannelBlockDecision::Blocked),
      mReason(TRACKING_PROTECTION) {
  MOZ_ASSERT(aChannel);
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetReason(uint8_t* aReason) {
  NS_ENSURE_ARG_POINTER(aReason);

  *aReason = mReason;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetUrl(nsAString& aUrl) {
  nsCOMPtr<nsIURI> uri;
  mChannel->GetURI(getter_AddRefs(uri));
  if (uri) {
    CopyUTF8toUTF16(uri->GetSpecOrDefault(), aUrl);
  }
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetTabId(uint64_t* aTabId) {
  NS_ENSURE_ARG_POINTER(aTabId);

  *aTabId = 0;

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
  MOZ_ASSERT(loadInfo);

  RefPtr<dom::BrowsingContext> browsingContext;
  nsresult rv =
      loadInfo->GetTargetBrowsingContext(getter_AddRefs(browsingContext));
  if (NS_WARN_IF(NS_FAILED(rv)) || !browsingContext) {
    return NS_ERROR_FAILURE;
  }

  // Get top-level browsing context to ensure window global parent is ready
  // to use, tabId is the same anyway.
  dom::CanonicalBrowsingContext* top = browsingContext->Canonical()->Top();
  dom::WindowGlobalParent* wgp = top->GetCurrentWindowGlobal();
  if (!wgp) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<dom::BrowserParent> browserParent = wgp->GetBrowserParent();
  if (!browserParent) {
    return NS_ERROR_FAILURE;
  }

  *aTabId = browserParent->GetTabId();
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetChannelId(uint64_t* aChannelId) {
  NS_ENSURE_ARG_POINTER(aChannelId);

  nsCOMPtr<nsIIdentChannel> channel(do_QueryInterface(mChannel));
  *aChannelId = channel ? channel->ChannelId() : 0;

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetTopLevelUrl(nsAString& aTopLevelUrl) {
  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
  MOZ_ASSERT(loadInfo);

  RefPtr<dom::BrowsingContext> browsingContext;
  nsresult rv =
      loadInfo->GetTargetBrowsingContext(getter_AddRefs(browsingContext));
  if (NS_WARN_IF(NS_FAILED(rv)) || !browsingContext) {
    return NS_ERROR_FAILURE;
  }

  // Get top-level browsing context to ensure window global parent is ready
  // to use, tabId is the same anyway.
  dom::CanonicalBrowsingContext* top = browsingContext->Canonical()->Top();
  dom::WindowGlobalParent* wgp = top->GetCurrentWindowGlobal();
  if (!wgp) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsIURI> uri = wgp->GetDocumentURI();
  if (!uri) {
    return NS_ERROR_FAILURE;
  }

  CopyUTF8toUTF16(uri->GetSpecOrDefault(), aTopLevelUrl);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetTables(nsACString& aTables) {
  aTables.Assign(mTables);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::GetIsPrivateBrowsing(bool* aIsPrivateBrowsing) {
  NS_ENSURE_ARG_POINTER(aIsPrivateBrowsing);

  *aIsPrivateBrowsing = NS_UsePrivateBrowsing(mChannel);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::Allow() {
  UC_LOG(("ChannelClassifierService: allow loading the channel %p",
          mChannel.get()));

  mDecision = ChannelBlockDecision::Allowed;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierBlockedChannel::Replace() {
  UC_LOG(("ChannelClassifierService: replace channel %p", mChannel.get()));

  mDecision = ChannelBlockDecision::Replaced;
  return NS_OK;
}

void UrlClassifierBlockedChannel::SetReason(const nsACString& aFeatureName,
                                            const nsACString& aTableName) {
  mTables = aTableName;

  nsCOMPtr<nsIUrlClassifierFeature> feature;
  feature =
      UrlClassifierFeatureTrackingProtection::GetIfNameMatches(aFeatureName);
  if (feature) {
    mReason = TRACKING_PROTECTION;
    return;
  }

  feature = UrlClassifierFeatureSocialTrackingProtection::GetIfNameMatches(
      aFeatureName);
  if (feature) {
    mReason = SOCIAL_TRACKING_PROTECTION;
    return;
  }

  feature = UrlClassifierFeatureFingerprintingProtection::GetIfNameMatches(
      aFeatureName);
  if (feature) {
    mReason = FINGERPRINTING_PROTECTION;
    return;
  }

  feature = UrlClassifierFeatureCryptominingProtection::GetIfNameMatches(
      aFeatureName);
  if (feature) {
    mReason = CRYPTOMINING_PROTECTION;
    return;
  }
}

NS_IMPL_ISUPPORTS(ChannelClassifierService, nsIChannelClassifierService)

// static
already_AddRefed<nsIChannelClassifierService>
ChannelClassifierService::GetSingleton() {
  if (gChannelClassifierService) {
    return do_AddRef(gChannelClassifierService);
  }

  gChannelClassifierService = new ChannelClassifierService();
  ClearOnShutdown(&gChannelClassifierService);
  return do_AddRef(gChannelClassifierService);
}

ChannelClassifierService::ChannelClassifierService() { mListeners.Clear(); }

NS_IMETHODIMP
ChannelClassifierService::AddListener(nsIObserver* aObserver) {
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(!mListeners.Contains(aObserver));

  mListeners.AppendElement(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
ChannelClassifierService::RemoveListener(nsIObserver* aObserver) {
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(mListeners.Contains(aObserver));

  mListeners.RemoveElement(aObserver);
  return NS_OK;
}

/* static */
ChannelBlockDecision ChannelClassifierService::OnBeforeBlockChannel(
    nsIChannel* aChannel, const nsACString& aFeatureName,
    const nsACString& aTableName) {
  MOZ_ASSERT(aChannel);

  // Don't bother continuing if no one has ever registered listener
  if (!gChannelClassifierService || !gChannelClassifierService->HasListener()) {
    return ChannelBlockDecision::Blocked;
  }

  ChannelBlockDecision decision;
  nsresult rv = gChannelClassifierService->OnBeforeBlockChannel(
      aChannel, aFeatureName, aTableName, decision);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ChannelBlockDecision::Blocked;
  }

  return decision;
}

nsresult ChannelClassifierService::OnBeforeBlockChannel(
    nsIChannel* aChannel, const nsACString& aFeatureName,
    const nsACString& aTableName, ChannelBlockDecision& aDecision) {
  MOZ_ASSERT(aChannel);

  aDecision = ChannelBlockDecision::Blocked;

  RefPtr<UrlClassifierBlockedChannel> channel =
      new UrlClassifierBlockedChannel(aChannel);
  channel->SetReason(aFeatureName, aTableName);

  for (const auto& listener : mListeners) {
    listener->Observe(
        NS_ISUPPORTS_CAST(nsIUrlClassifierBlockedChannel*, channel),
        "urlclassifier-before-block-channel", nullptr);

    aDecision = channel->GetDecision();
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
