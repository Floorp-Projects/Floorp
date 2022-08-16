/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureEmailTrackingDataCollection.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"

#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsILoadInfo.h"

namespace mozilla::net {

namespace {

#define EMAILTRACKING_DATACOLLECTION_FEATURE_NAME \
  "emailtracking-data-collection"

#define URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_BLOCKLIST \
  "urlclassifier.features.emailtracking.datacollection.blocklistTables"
#define URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.emailtracking.datacollection.blocklistHosts"
#define URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_ENTITYLIST \
  "urlclassifier.features.emailtracking.datacollection.allowlistTables"
#define URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.emailtracking.datacollection.allowlistHosts"
#define URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_EXCEPTION_URLS \
  "urlclassifier.features.emailtracking.datacollection.skipURLs"
#define TABLE_EMAILTRACKING_DATACOLLECTION_BLOCKLIST_PREF \
  "emailtracking-data-collection-blocklist-pref"
#define TABLE_EMAILTRACKING_DATACOLLECTION_ENTITYLIST_PREF \
  "emailtracking-data-collection-allowlist-pref"

StaticRefPtr<UrlClassifierFeatureEmailTrackingDataCollection>
    gFeatureEmailTrackingDataCollection;
StaticAutoPtr<nsCString> gEmailWebAppDomainsPref;
static constexpr char kEmailWebAppDomainPrefName[] =
    "privacy.trackingprotection.emailtracking.webapp.domains";

void EmailWebAppDomainPrefChangeCallback(const char* aPrefName, void*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kEmailWebAppDomainPrefName));
  MOZ_ASSERT(gEmailWebAppDomainsPref);

  Preferences::GetCString(kEmailWebAppDomainPrefName, *gEmailWebAppDomainsPref);
}

}  // namespace

UrlClassifierFeatureEmailTrackingDataCollection::
    UrlClassifierFeatureEmailTrackingDataCollection()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(EMAILTRACKING_DATACOLLECTION_FEATURE_NAME),
          nsLiteralCString(
              URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_BLOCKLIST),
          nsLiteralCString(
              URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_EMAILTRACKING_DATACOLLECTION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_EMAILTRACKING_DATACOLLECTION_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_EMAILTRACKING_DATACOLLECTION_EXCEPTION_URLS)) {}

/* static */
const char* UrlClassifierFeatureEmailTrackingDataCollection::Name() {
  return EMAILTRACKING_DATACOLLECTION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureEmailTrackingDataCollection::MaybeInitialize() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureEmailTrackingDataCollection::MaybeInitialize"));

  if (!gFeatureEmailTrackingDataCollection) {
    gFeatureEmailTrackingDataCollection =
        new UrlClassifierFeatureEmailTrackingDataCollection();
    gFeatureEmailTrackingDataCollection->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureEmailTrackingDataCollection::MaybeShutdown() {
  UC_LOG_LEAK(
      ("UrlClassifierFeatureEmailTrackingDataCollection::MaybeShutdown"));

  if (gFeatureEmailTrackingDataCollection) {
    gFeatureEmailTrackingDataCollection->ShutdownPreferences();
    gFeatureEmailTrackingDataCollection = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureEmailTrackingDataCollection>
UrlClassifierFeatureEmailTrackingDataCollection::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureEmailTrackingDataCollection::MaybeCreate - channel "
       "%p",
       aChannel));

  if (!StaticPrefs::
          privacy_trackingprotection_emailtracking_data_collection_enabled()) {
    return nullptr;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);

  // We don't need to collect data if the email tracker is loaded in first-party
  // context.
  if (!isThirdParty) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureEmailTrackingDataCollection);

  RefPtr<UrlClassifierFeatureEmailTrackingDataCollection> self =
      gFeatureEmailTrackingDataCollection;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureEmailTrackingDataCollection::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(EMAILTRACKING_DATACOLLECTION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureEmailTrackingDataCollection);

  RefPtr<UrlClassifierFeatureEmailTrackingDataCollection> self =
      gFeatureEmailTrackingDataCollection;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureEmailTrackingDataCollection::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureEmailTrackingDataCollection::ProcessChannel - "
       "collecting data from channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"base-email-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_EMAILTRACKING},
          {"content-email-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_EMAILTRACKING_CONTENT},
      };

  // Get if the top window is a email webapp.
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  RefPtr<dom::BrowsingContext> bc;
  loadInfo->GetBrowsingContext(getter_AddRefs(bc));
  if (!bc || bc->IsDiscarded()) {
    return NS_OK;
  }

  RefPtr<dom::WindowGlobalParent> topWindowParent =
      bc->Canonical()->GetTopWindowContext();
  if (!topWindowParent || topWindowParent->IsDiscarded()) {
    return NS_OK;
  }

  // Cache the email webapp domains pref value and register the callback
  // function to update the cached value when the pref changes.
  if (!gEmailWebAppDomainsPref) {
    gEmailWebAppDomainsPref = new nsCString();

    Preferences::RegisterCallbackAndCall(EmailWebAppDomainPrefChangeCallback,
                                         kEmailWebAppDomainPrefName);
    RunOnShutdown([]() {
      Preferences::UnregisterCallback(EmailWebAppDomainPrefChangeCallback,
                                      kEmailWebAppDomainPrefName);
      gEmailWebAppDomainsPref = nullptr;
    });
  }

  bool isTopEmailWebApp = topWindowParent->DocumentPrincipal()->IsURIInList(
      *gEmailWebAppDomainsPref);

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_EMAILTRACKING);

  if (flags & nsIClassifiedChannel::ClassificationFlags::
                  CLASSIFIED_EMAILTRACKING_CONTENT) {
    Telemetry::AccumulateCategorical(
        isTopEmailWebApp
            ? Telemetry::LABELS_EMAIL_TRACKER_COUNT::content_email_webapp
            : Telemetry::LABELS_EMAIL_TRACKER_COUNT::content_normal);

    // Notify the load event to record the content blocking log.
    //
    // Note that we need to change the code here if we decided to block content
    // email trackers in the future.
    ContentBlockingNotifier::OnEvent(
        aChannel,
        nsIWebProgressListener::STATE_LOADED_EMAILTRACKING_LEVEL_2_CONTENT);
  } else {
    Telemetry::AccumulateCategorical(
        isTopEmailWebApp
            ? Telemetry::LABELS_EMAIL_TRACKER_COUNT::base_email_webapp
            : Telemetry::LABELS_EMAIL_TRACKER_COUNT::base_normal);
    // Notify to record content blocking log. Note that we don't need to notify
    // if email tracking is enabled because the email tracking protection
    // feature will be responsible for notifying the blocking event.
    //
    // Note that we need to change the code here if we decided to block content
    // email trackers in the future.
    if (!StaticPrefs::privacy_trackingprotection_emailtracking_enabled()) {
      ContentBlockingNotifier::OnEvent(
          aChannel,
          nsIWebProgressListener::STATE_LOADED_EMAILTRACKING_LEVEL_1_CONTENT);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureEmailTrackingDataCollection::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);

  if (aListType == nsIUrlClassifierFeature::blocklist) {
    *aURIType = nsIUrlClassifierFeature::blocklistURI;
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::entitylist);

  *aURIType = nsIUrlClassifierFeature::pairwiseEntitylistURI;
  return UrlClassifierCommon::CreatePairwiseEntityListURI(aChannel, aURI);
}

}  // namespace mozilla::net
