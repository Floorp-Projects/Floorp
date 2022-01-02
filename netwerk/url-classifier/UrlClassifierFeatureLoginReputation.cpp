/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureLoginReputation.h"

#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPtr.h"
#include "UrlClassifierCommon.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

namespace {

#define LOGIN_REPUTATION_FEATURE_NAME "login-reputation"

#define PREF_PASSWORD_ALLOW_TABLE "urlclassifier.passwordAllowTable"

StaticRefPtr<UrlClassifierFeatureLoginReputation> gFeatureLoginReputation;

}  // namespace

UrlClassifierFeatureLoginReputation::UrlClassifierFeatureLoginReputation()
    : UrlClassifierFeatureBase(nsLiteralCString(LOGIN_REPUTATION_FEATURE_NAME),
                               ""_ns,  // blocklist tables
                               nsLiteralCString(PREF_PASSWORD_ALLOW_TABLE),
                               ""_ns,  // blocklist pref
                               ""_ns,  // entitylist pref
                               ""_ns,  // blocklist pref table name
                               ""_ns,  // entitylist pref table name
                               ""_ns)  // exception host pref
{}

/* static */ const char* UrlClassifierFeatureLoginReputation::Name() {
  return StaticPrefs::browser_safebrowsing_passwords_enabled()
             ? LOGIN_REPUTATION_FEATURE_NAME
             : "";
}

/* static */
void UrlClassifierFeatureLoginReputation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureLoginReputation::MaybeShutdown"));

  if (gFeatureLoginReputation) {
    gFeatureLoginReputation->ShutdownPreferences();
    gFeatureLoginReputation = nullptr;
  }
}

/* static */
nsIUrlClassifierFeature*
UrlClassifierFeatureLoginReputation::MaybeGetOrCreate() {
  if (!StaticPrefs::browser_safebrowsing_passwords_enabled()) {
    return nullptr;
  }

  if (!gFeatureLoginReputation) {
    gFeatureLoginReputation = new UrlClassifierFeatureLoginReputation();
    gFeatureLoginReputation->InitializePreferences();
  }

  return gFeatureLoginReputation;
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureLoginReputation::GetIfNameMatches(const nsACString& aName) {
  if (!aName.EqualsLiteral(LOGIN_REPUTATION_FEATURE_NAME)) {
    return nullptr;
  }

  nsCOMPtr<nsIUrlClassifierFeature> self = MaybeGetOrCreate();
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureLoginReputation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  MOZ_CRASH(
      "UrlClassifierFeatureLoginReputation::ProcessChannel should never be "
      "called");
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureLoginReputation::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);
  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::entitylist,
             "UrlClassifierFeatureLoginReputation is meant to be used just to "
             "entitylist URLs");
  *aURIType = nsIUrlClassifierFeature::URIType::entitylistURI;
  return aChannel->GetURI(aURI);
}

}  // namespace net
}  // namespace mozilla
