/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureFlash.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/StaticPrefs_plugins.h"
#include "nsIXULRuntime.h"
#include "nsScriptSecurityManager.h"
#include "nsQueryObject.h"
#include "UrlClassifierCommon.h"
#include "nsIParentChannel.h"

namespace mozilla {
namespace net {

struct UrlClassifierFeatureFlash::FlashFeature {
  const char* mName;
  const char* mBlocklistPrefTables;
  const char* mEntitylistPrefTables;
  bool mSubdocumentOnly;
  nsIHttpChannel::FlashPluginState mFlashPluginState;
  RefPtr<UrlClassifierFeatureFlash> mFeature;
};

namespace {

static UrlClassifierFeatureFlash::FlashFeature sFlashFeaturesMap[] = {
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

UrlClassifierFeatureFlash::UrlClassifierFeatureFlash(
    const UrlClassifierFeatureFlash::FlashFeature& aFlashFeature)
    : UrlClassifierFeatureBase(
          nsDependentCString(aFlashFeature.mName),
          nsDependentCString(aFlashFeature.mBlocklistPrefTables),
          nsDependentCString(aFlashFeature.mEntitylistPrefTables),
          ""_ns,  // aPrefBlocklistHosts
          ""_ns,  // aPrefEntitylistHosts
          ""_ns,  // aPrefBlocklistTableName
          ""_ns,  // aPrefEntitylistTableName
          ""_ns)  // aPrefExceptionHosts
      ,
      mFlashPluginState(aFlashFeature.mFlashPluginState) {
  static_assert(nsIHttpChannel::FlashPluginDeniedInSubdocuments ==
                    nsIHttpChannel::FlashPluginLastValue,
                "nsIHttpChannel::FlashPluginLastValue is out-of-sync!");
}

/* static */
void UrlClassifierFeatureFlash::GetFeatureNames(nsTArray<nsCString>& aArray) {
  for (const FlashFeature& flashFeature : sFlashFeaturesMap) {
    aArray.AppendElement(nsDependentCString(flashFeature.mName));
  }
}

/* static */
void UrlClassifierFeatureFlash::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (IsInitialized()) {
    return;
  }

  for (FlashFeature& flashFeature : sFlashFeaturesMap) {
    MOZ_ASSERT(!flashFeature.mFeature);
    flashFeature.mFeature = new UrlClassifierFeatureFlash(flashFeature);
    flashFeature.mFeature->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureFlash::MaybeShutdown() {
  if (!IsInitialized()) {
    return;
  }

  for (FlashFeature& flashFeature : sFlashFeaturesMap) {
    MOZ_ASSERT(flashFeature.mFeature);
    flashFeature.mFeature->ShutdownPreferences();
    flashFeature.mFeature = nullptr;
  }
}

/* static */
void UrlClassifierFeatureFlash::MaybeCreate(
    nsIChannel* aChannel,
    nsTArray<nsCOMPtr<nsIUrlClassifierFeature>>& aFeatures) {
  const auto fnIsFlashBlockingEnabled = [] {
    return StaticPrefs::plugins_flashBlock_enabled() && !FissionAutostart();
  };

  // All disabled.
  if (!fnIsFlashBlockingEnabled()) {
    return;
  }

  // We use Flash feature just for document loading.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  ExtContentPolicyType contentPolicyType =
      loadInfo->GetExternalContentPolicyType();

  if (contentPolicyType != ExtContentPolicy::TYPE_DOCUMENT &&
      contentPolicyType != ExtContentPolicy::TYPE_SUBDOCUMENT) {
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

  for (const FlashFeature& flashFeature : sFlashFeaturesMap) {
    MOZ_ASSERT(flashFeature.mFeature);
    if (!flashFeature.mSubdocumentOnly ||
        contentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
      aFeatures.AppendElement(flashFeature.mFeature);
    }
  }
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureFlash::GetIfNameMatches(const nsACString& aName) {
  MaybeInitialize();

  for (const FlashFeature& flashFeature : sFlashFeaturesMap) {
    MOZ_ASSERT(flashFeature.mFeature);
    if (aName.Equals(flashFeature.mName)) {
      nsCOMPtr<nsIUrlClassifierFeature> self = flashFeature.mFeature.get();
      return self.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
UrlClassifierFeatureFlash::ProcessChannel(nsIChannel* aChannel,
                                          const nsTArray<nsCString>& aList,
                                          const nsTArray<nsCString>& aHashes,
                                          bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(("UrlClassifierFeatureFlash::ProcessChannel - annotating channel %p",
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
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);

  // Here we return the channel's URI always.
  *aURIType = aListType == nsIUrlClassifierFeature::blocklist
                  ? nsIUrlClassifierFeature::URIType::blocklistURI
                  : nsIUrlClassifierFeature::URIType::entitylistURI;
  return aChannel->GetURI(aURI);
}

}  // namespace net
}  // namespace mozilla
