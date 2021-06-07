/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLQueryStringStripper.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"

#include "nsEffectiveTLDService.h"
#include "nsIPrefBranch.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsUnicharUtils.h"
#include "nsURLHelper.h"

namespace {
static const char kPRefQueryStrippingList[] =
    "privacy.query_stripping.strip_list";
static const char kPRefQueryStrippingAllowList[] =
    "privacy.query_stripping.allow_list";

mozilla::StaticRefPtr<mozilla::URLQueryStringStripper> gQueryStringStripper;

}  // namespace

namespace mozilla {

NS_IMPL_ISUPPORTS(URLQueryStringStripper, nsIObserver)

URLQueryStringStripper* URLQueryStringStripper::GetOrCreate() {
  if (!gQueryStringStripper) {
    gQueryStringStripper = new URLQueryStringStripper();
    gQueryStringStripper->Init();

    RunOnShutdown([&] {
      gQueryStringStripper->Shutdown();
      gQueryStringStripper = nullptr;
    });
  }

  return gQueryStringStripper;
}

NS_IMETHODIMP
URLQueryStringStripper::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);

    if (data.EqualsLiteral(kPRefQueryStrippingList)) {
      PopulateStripList();
    }

    if (data.EqualsLiteral(kPRefQueryStrippingAllowList)) {
      PopulateAllowList();
    }
  }
  return NS_OK;
}

/* static */
bool URLQueryStringStripper::Strip(nsIURI* aURI, nsCOMPtr<nsIURI>& aOutput) {
  if (!StaticPrefs::privacy_query_stripping_enabled()) {
    return false;
  }

  RefPtr<URLQueryStringStripper> stripper = GetOrCreate();

  if (stripper->CheckAllowList(aURI)) {
    return false;
  }

  return stripper->StripQueryString(aURI, aOutput);
}

void URLQueryStringStripper::Init() {
  Preferences::AddStrongObserver(this, kPRefQueryStrippingList);
  Preferences::AddStrongObserver(this, kPRefQueryStrippingAllowList);

  PopulateStripList();
  PopulateAllowList();
}

void URLQueryStringStripper::Shutdown() {
  Preferences::RemoveObserver(this, kPRefQueryStrippingList);
  Preferences::RemoveObserver(this, kPRefQueryStrippingAllowList);
  mList.Clear();
}

bool URLQueryStringStripper::StripQueryString(nsIURI* aURI,
                                              nsCOMPtr<nsIURI>& aOutput) {
  MOZ_ASSERT(aURI);

  nsCOMPtr<nsIURI> uri(aURI);

  nsAutoCString query;
  nsresult rv = aURI->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, false);

  // We don't need to do anything if there is no query string.
  if (query.IsEmpty()) {
    return false;
  }

  URLParams params;
  bool hasStripped = false;

  URLParams::Parse(query, [&](nsString&& name, nsString&& value) {
    nsAutoString lowerCaseName;

    ToLowerCase(name, lowerCaseName);

    if (mList.Contains(lowerCaseName)) {
      hasStripped = true;
      return true;
    }

    params.Append(name, value);
    return true;
  });

  // Return if there is no parameter has been stripped.
  if (!hasStripped) {
    return false;
  }

  nsAutoString newQuery;
  params.Serialize(newQuery);

  Unused << NS_MutateURI(uri)
                .SetQuery(NS_ConvertUTF16toUTF8(newQuery))
                .Finalize(aOutput);

  return true;
}

bool URLQueryStringStripper::CheckAllowList(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  // Get the site(eTLD+1) from the URI.
  nsAutoCString baseDomain;
  nsresult rv =
      nsEffectiveTLDService::GetInstance()->GetBaseDomain(aURI, 0, baseDomain);
  NS_ENSURE_SUCCESS(rv, false);

  return mAllowList.Contains(baseDomain);
}

void URLQueryStringStripper::PopulateStripList() {
  nsAutoString stripList;
  Preferences::GetString(kPRefQueryStrippingList, stripList);
  ToLowerCase(stripList);

  mList.Clear();

  for (const nsAString& item : stripList.Split(' ')) {
    mList.Insert(item);
  }
}

void URLQueryStringStripper::PopulateAllowList() {
  nsAutoCString allowList;
  Preferences::GetCString(kPRefQueryStrippingAllowList, allowList);
  ToLowerCase(allowList);

  mAllowList.Clear();

  for (const nsACString& item : allowList.Split(',')) {
    mAllowList.Insert(item);
  }
}

}  // namespace mozilla
