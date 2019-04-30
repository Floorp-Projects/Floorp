/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureLoginReputation.h"

#include "mozilla/StaticPrefs.h"

namespace mozilla {
namespace net {

namespace {

#define LOGIN_REPUTATION_FEATURE_NAME "login-reputation"

#define PREF_PASSWORD_ALLOW_TABLE "urlclassifier.passwordAllowTable"

StaticRefPtr<UrlClassifierFeatureLoginReputation> gFeatureLoginReputation;

}  // namespace

UrlClassifierFeatureLoginReputation::UrlClassifierFeatureLoginReputation()
    : UrlClassifierFeatureBase(
          NS_LITERAL_CSTRING(LOGIN_REPUTATION_FEATURE_NAME),
          EmptyCString(),  // blacklist tables
          NS_LITERAL_CSTRING(PREF_PASSWORD_ALLOW_TABLE),
          EmptyCString(),  // blacklist pref
          EmptyCString(),  // whitelist pref
          EmptyCString(),  // blacklist pref table name
          EmptyCString(),  // whitelist pref table name
          EmptyCString())  // skip host pref
{}

/* static */ const char* UrlClassifierFeatureLoginReputation::Name() {
  return StaticPrefs::browser_safebrowsing_passwords_enabled()
             ? LOGIN_REPUTATION_FEATURE_NAME
             : "";
}

/* static */
void UrlClassifierFeatureLoginReputation::MaybeShutdown() {
  UC_LOG(("UrlClassifierFeatureLoginReputation: MaybeShutdown"));

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
    nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURI);
  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::whitelist,
             "UrlClassifierFeatureLoginReputation is meant to be used just to "
             "whitelist URLs");

  return aChannel->GetURI(aURI);
}

}  // namespace net
}  // namespace mozilla
