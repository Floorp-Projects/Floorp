/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFlash.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsScriptSecurityManager.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

namespace {

struct FlashFeatures {
  const char* mName;
  const char* mBlacklistPrefTables;
  const char* mWhitelistPrefTables;
  bool mSubdocumentOnly;
  nsIHttpChannel::FlashPluginState mFlashPluginState;
  RefPtr<UrlClassifierFeatureFlash> mFeature;
};

static FlashFeatures sFlashFeaturesMap[] = {
    {"flash-deny", "urlclassifier.flashTable", "urlclassifier.flashExceptTable",
     false, nsIHttpChannel::FlashPluginDenied},
    {"flash-allow", "urlclassifier.flashAllowTable",
     "urlclassifier.flashAllowExceptTable", false,
     nsIHttpChannel::FlashPluginAllowed},
    {"flash-deny-subdoc", "urlclassifier.flashSubDocTable",
     "urlclassifier.flashSubDocExceptTable", true,
     nsIHttpChannel::FlashPluginDeniedInSubdocuments},
};

bool IsInitialized() { return !!sFlashFeaturesMap[0].mFeature; }

}  // namespace

UrlClassifierFeatureFlash::UrlClassifierFeatureFlash(uint32_t aId)
    : UrlClassifierFeatureBase(
          nsDependentCString(sFlashFeaturesMap[aId].mName),
          nsDependentCString(sFlashFeaturesMap[aId].mBlacklistPrefTables),
          nsDependentCString(sFlashFeaturesMap[aId].mWhitelistPrefTables),
          EmptyCString(),  // aPrefBlacklistHosts
          EmptyCString(),  // aPrefWhitelistHosts
          EmptyCString(),  // aPrefBlacklistTableName
          EmptyCString(),  // aPrefWhitelistTableName
          EmptyCString())  // aPrefSkipHosts
      ,
      mFlashPluginState(sFlashFeaturesMap[aId].mFlashPluginState) {
  static_assert(nsIHttpChannel::FlashPluginDeniedInSubdocuments ==
                    nsIHttpChannel::FlashPluginLastValue,
                "nsIHttpChannel::FlashPluginLastValue is out-of-sync!");
}

/* static */ void UrlClassifierFeatureFlash::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (IsInitialized()) {
    return;
  }

  uint32_t numFeatures =
      (sizeof(sFlashFeaturesMap) / sizeof(sFlashFeaturesMap[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    MOZ_ASSERT(!sFlashFeaturesMap[i].mFeature);
    sFlashFeaturesMap[i].mFeature = new UrlClassifierFeatureFlash(i);
    sFlashFeaturesMap[i].mFeature->InitializePreferences();
  }
}

/* static */ void UrlClassifierFeatureFlash::MaybeShutdown() {
  if (!IsInitialized()) {
    return;
  }

  uint32_t numFeatures =
      (sizeof(sFlashFeaturesMap) / sizeof(sFlashFeaturesMap[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    MOZ_ASSERT(sFlashFeaturesMap[i].mFeature);
    sFlashFeaturesMap[i].mFeature->ShutdownPreferences();
    sFlashFeaturesMap[i].mFeature = nullptr;
  }
}

/* static */ void UrlClassifierFeatureFlash::MaybeCreate(
    nsIChannel* aChannel,
    nsTArray<nsCOMPtr<nsIUrlClassifierFeature>>& aFeatures) {
  // All disabled.
  if (!StaticPrefs::plugins_flashBlock_enabled()) {
    return;
  }

  // We use Flash feature just for document loading.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  nsContentPolicyType contentPolicyType =
      loadInfo ? loadInfo->GetExternalContentPolicyType()
               : nsIContentPolicy::TYPE_INVALID;

  if (contentPolicyType != nsIContentPolicy::TYPE_DOCUMENT &&
      contentPolicyType != nsIContentPolicy::TYPE_SUBDOCUMENT) {
    return;
  }

  // Only allow plugins for documents from an HTTP/HTTPS origin.
  if (StaticPrefs::plugins_http_https_only()) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
    if (!httpChannel) {
      return;
    }
  }

  MaybeInitialize();

  uint32_t numFeatures =
      (sizeof(sFlashFeaturesMap) / sizeof(sFlashFeaturesMap[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    MOZ_ASSERT(sFlashFeaturesMap[i].mFeature);
    if (!sFlashFeaturesMap[i].mSubdocumentOnly ||
        contentPolicyType == nsIContentPolicy::TYPE_SUBDOCUMENT) {
      aFeatures.AppendElement(sFlashFeaturesMap[i].mFeature);
    }
  }
}

/* static */ already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFlash::GetIfNameMatches(const nsACString& aName) {
  MaybeInitialize();

  uint32_t numFeatures =
      (sizeof(sFlashFeaturesMap) / sizeof(sFlashFeaturesMap[0]));
  for (uint32_t i = 0; i < numFeatures; ++i) {
    MOZ_ASSERT(sFlashFeaturesMap[i].mFeature);
    if (aName.Equals(sFlashFeaturesMap[i].mName)) {
      nsCOMPtr<nsIUrlClassifierFeature> self =
          sFlashFeaturesMap[i].mFeature.get();
      return self.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
UrlClassifierFeatureFlash::ProcessChannel(nsIChannel* aChannel,
                                          const nsACString& aList,
                                          bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(("UrlClassifierFeatureFlash::ProcessChannel, annotating channel[%p]",
          aChannel));

  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process
    // request. We should notify the child process as well.
    parentChannel->NotifyFlashPluginStateChanged(mFlashPluginState);
  }

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(aChannel);
  if (httpChannel) {
    httpChannel->SetFlashPluginState(mFlashPluginState);
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureFlash::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURI);

  // Here we return the channel's URI always.
  return aChannel->GetURI(aURI);
}

}  // namespace net
}  // namespace mozilla
