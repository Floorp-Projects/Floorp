/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLQueryStringStripper.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Telemetry.h"

#include "nsEffectiveTLDService.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsUnicharUtils.h"
#include "nsURLHelper.h"
#include "mozilla/dom/StripOnShareRuleBinding.h"

namespace {

mozilla::StaticRefPtr<mozilla::URLQueryStringStripper> gQueryStringStripper;

static const char kQueryStrippingEnabledPref[] =
    "privacy.query_stripping.enabled";
static const char kQueryStrippingEnabledPBMPref[] =
    "privacy.query_stripping.enabled.pbmode";
static const char kQueryStrippingOnShareEnabledPref[] =
    "privacy.query_stripping.strip_on_share.enabled";

}  // namespace

namespace mozilla {

NS_IMPL_ISUPPORTS(URLQueryStringStripper, nsIObserver,
                  nsIURLQueryStringStripper, nsIURLQueryStrippingListObserver)

// static
already_AddRefed<URLQueryStringStripper>
URLQueryStringStripper::GetSingleton() {
  if (!gQueryStringStripper) {
    gQueryStringStripper = new URLQueryStringStripper();
    // Check initial pref state and enable service. We can pass nullptr, because
    // OnPrefChange doesn't rely on the args.
    URLQueryStringStripper::OnPrefChange(nullptr, nullptr);

    RunOnShutdown(
        [&] {
          DebugOnly<nsresult> rv = gQueryStringStripper->Shutdown();
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "URLQueryStringStripper::Shutdown failed");
          gQueryStringStripper = nullptr;
        },
        ShutdownPhase::XPCOMShutdown);
  }

  return do_AddRef(gQueryStringStripper);
}

URLQueryStringStripper::URLQueryStringStripper() {
  mIsInitialized = false;

  nsresult rv = Preferences::RegisterCallback(
      &URLQueryStringStripper::OnPrefChange, kQueryStrippingEnabledPBMPref);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = Preferences::RegisterCallback(&URLQueryStringStripper::OnPrefChange,
                                     kQueryStrippingEnabledPref);

  rv = Preferences::RegisterCallback(&URLQueryStringStripper::OnPrefChange,
                                     kQueryStrippingOnShareEnabledPref);
  NS_ENSURE_SUCCESS_VOID(rv);
}

NS_IMETHODIMP
URLQueryStringStripper::StripForCopyOrShare(nsIURI* aURI,
                                            nsIURI** strippedURI) {
  if (!StaticPrefs::privacy_query_stripping_strip_on_share_enabled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(strippedURI);
  int aStripCount = 0;

  nsAutoCString query;
  nsresult rv = aURI->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, rv);
  // We don't need to do anything if there is no query string.
  if (query.IsEmpty()) {
    Telemetry::Accumulate(Telemetry::STRIP_ON_SHARE_PARAMS_REMOVED, 0);
    return NS_OK;
  }
  nsAutoCString host;
  rv = aURI->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  URLParams params;

  URLParams::Parse(query, [&](nsString&& name, nsString&& value) {
    nsAutoString lowerCaseName;
    ToLowerCase(name, lowerCaseName);
    // Look through the global rules.
    dom::StripRule globalRule;
    bool keyExists = mStripOnShareMap.Get("*"_ns, &globalRule);
    // There should always be a global rule.
    MOZ_ASSERT(keyExists);
    for (const auto& param : globalRule.mQueryParams) {
      if (param == lowerCaseName) {
        aStripCount++;
        return true;
      }
    }

    // Check for site specific rules.
    dom::StripRule siteSpecificRule;
    keyExists = mStripOnShareMap.Get(host, &siteSpecificRule);
    if (keyExists) {
      for (const auto& param : siteSpecificRule.mQueryParams) {
        if (param == lowerCaseName) {
          aStripCount++;
          return true;
        }
      }
    }

    params.Append(name, value);
    return true;
  });

  Telemetry::Accumulate(Telemetry::STRIP_ON_SHARE_PARAMS_REMOVED, aStripCount);

  if (!aStripCount) {
    return NS_OK;
  }

  nsAutoString newQuery;
  params.Serialize(newQuery, false);

  Unused << NS_MutateURI(aURI)
                .SetQuery(NS_ConvertUTF16toUTF8(newQuery))
                .Finalize(strippedURI);

  // To calculate difference in length of the URL
  // after stripping occurs for Telemetry
  nsAutoCString specOriginalURI;
  nsAutoCString specStrippedURI;

  rv = aURI->GetDisplaySpec(specOriginalURI);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(*strippedURI);

  rv = (*strippedURI)->GetDisplaySpec(specStrippedURI);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t lengthDiff = specOriginalURI.Length() - specStrippedURI.Length();
  Telemetry::Accumulate(Telemetry::STRIP_ON_SHARE_LENGTH_DECREASE, lengthDiff);

  return NS_OK;
}

NS_IMETHODIMP
URLQueryStringStripper::Strip(nsIURI* aURI, bool aIsPBM, nsIURI** aOutput,
                              uint32_t* aStripCount) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aOutput);
  NS_ENSURE_ARG_POINTER(aStripCount);

  *aStripCount = 0;

  if (aIsPBM) {
    if (!StaticPrefs::privacy_query_stripping_enabled_pbmode()) {
      return NS_OK;
    }
  } else {
    if (!StaticPrefs::privacy_query_stripping_enabled()) {
      return NS_OK;
    }
  }

  if (CheckAllowList(aURI)) {
    return NS_OK;
  }

  return StripQueryString(aURI, aOutput, aStripCount);
}

// static
void URLQueryStringStripper::OnPrefChange(const char* aPref, void* aData) {
  MOZ_ASSERT(gQueryStringStripper);

  bool prefEnablesComponent =
      StaticPrefs::privacy_query_stripping_enabled() ||
      StaticPrefs::privacy_query_stripping_enabled_pbmode() ||
      StaticPrefs::privacy_query_stripping_strip_on_share_enabled();

  nsresult rv;
  if (prefEnablesComponent) {
    rv = gQueryStringStripper->Init();
  } else {
    rv = gQueryStringStripper->Shutdown();
  }
  NS_ENSURE_SUCCESS_VOID(rv);
}

nsresult URLQueryStringStripper::Init() {
  nsresult rv;
  if (mIsInitialized) {
    rv = gQueryStringStripper->ManageObservers();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }
  mIsInitialized = true;

  mListService = do_GetService("@mozilla.org/query-stripping-list-service;1");
  NS_ENSURE_TRUE(mListService, NS_ERROR_FAILURE);
  rv = gQueryStringStripper->ManageObservers();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// (Un)registers a QPS/Strip-on-share observer according to the QPS prefs states
// and the strip-on-share pref state. This is called whenever one of the three
// prefs changes, to ensure that we are not observing one of the lists although
// the corresponding feature is not turned on.
nsresult URLQueryStringStripper::ManageObservers() {
  MOZ_ASSERT(mListService);
  nsresult rv;
  // Register QPS observer.
  // We are not listening to QPS but the feature is on, register a listener.
  if (!mObservingQPS) {
    if (StaticPrefs::privacy_query_stripping_enabled() ||
        StaticPrefs::privacy_query_stripping_enabled_pbmode()) {
      rv = mListService->RegisterAndRunObserver(gQueryStringStripper);
      NS_ENSURE_SUCCESS(rv, rv);
      mObservingQPS = true;
    }
  } else {
    // Unregister QPS observer.
    // We are listening to QPS but the feature is off, unregister.
    if (!StaticPrefs::privacy_query_stripping_enabled() &&
        !StaticPrefs::privacy_query_stripping_enabled_pbmode()) {
      // Clean up QPS lists.
      mList.Clear();
      mAllowList.Clear();
      rv = mListService->UnregisterObserver(this);
      NS_ENSURE_SUCCESS(rv, rv);
      mObservingQPS = false;
    }
  }

  // Register Strip on Share observer.
  // We are not listening to strip-on-share but the feature is on, register an
  // Observer.
  if (!mObservingStripOnShare) {
    if (StaticPrefs::privacy_query_stripping_strip_on_share_enabled()) {
      rv = mListService->RegisterAndRunObserverStripOnShare(
          gQueryStringStripper);
      NS_ENSURE_SUCCESS(rv, rv);
      mObservingStripOnShare = true;
    }
  } else {
    // Unregister Strip on Share observer.
    // We are listening to strip-on-share but the feature is off, unregister.
    if (!StaticPrefs::privacy_query_stripping_strip_on_share_enabled()) {
      // Clean up strip-on-share list
      mStripOnShareMap.Clear();
      rv = mListService->UnregisterStripOnShareObserver(this);
      NS_ENSURE_SUCCESS(rv, rv);
      mObservingStripOnShare = false;
    }
  }
  return NS_OK;
}

nsresult URLQueryStringStripper::Shutdown() {
  if (!mIsInitialized) {
    return NS_OK;
  }
  nsresult rv = gQueryStringStripper->ManageObservers();
  NS_ENSURE_SUCCESS(rv, rv);
  mIsInitialized = false;
  mListService = nullptr;
  return NS_OK;
}

nsresult URLQueryStringStripper::StripQueryString(nsIURI* aURI,
                                                  nsIURI** aOutput,
                                                  uint32_t* aStripCount) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aOutput);
  NS_ENSURE_ARG_POINTER(aStripCount);

  *aStripCount = 0;

  nsCOMPtr<nsIURI> uri(aURI);

  nsAutoCString query;
  nsresult rv = aURI->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, rv);

  // We don't need to do anything if there is no query string.
  if (query.IsEmpty()) {
    return NS_OK;
  }

  URLParams params;

  URLParams::Parse(query, [&](nsString&& name, nsString&& value) {
    nsAutoString lowerCaseName;

    ToLowerCase(name, lowerCaseName);

    if (mList.Contains(lowerCaseName)) {
      *aStripCount += 1;

      // Count how often a specific query param is stripped. For privacy reasons
      // this will only count query params listed in the Histogram definition.
      // Calls for any other query params will be discarded.
      nsAutoCString telemetryLabel("param_");
      AppendUTF16toUTF8(lowerCaseName, telemetryLabel);
      Telemetry::AccumulateCategorical(
          Telemetry::QUERY_STRIPPING_COUNT_BY_PARAM, telemetryLabel);

      return true;
    }

    params.Append(name, value);
    return true;
  });

  // Return if there is no parameter has been stripped.
  if (!*aStripCount) {
    return NS_OK;
  }

  nsAutoString newQuery;
  params.Serialize(newQuery, false);

  Unused << NS_MutateURI(uri)
                .SetQuery(NS_ConvertUTF16toUTF8(newQuery))
                .Finalize(aOutput);

  return NS_OK;
}

bool URLQueryStringStripper::CheckAllowList(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  // Get the site(eTLD+1) from the URI.
  nsAutoCString baseDomain;
  nsresult rv =
      nsEffectiveTLDService::GetInstance()->GetBaseDomain(aURI, 0, baseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    return false;
  }
  NS_ENSURE_SUCCESS(rv, false);

  return mAllowList.Contains(baseDomain);
}

void URLQueryStringStripper::PopulateStripList(const nsAString& aList) {
  mList.Clear();

  for (const nsAString& item : aList.Split(' ')) {
    mList.Insert(item);
  }
}

void URLQueryStringStripper::PopulateAllowList(const nsACString& aList) {
  mAllowList.Clear();

  for (const nsACString& item : aList.Split(',')) {
    mAllowList.Insert(item);
  }
}

NS_IMETHODIMP
URLQueryStringStripper::OnQueryStrippingListUpdate(
    const nsAString& aStripList, const nsACString& aAllowList) {
  PopulateStripList(aStripList);
  PopulateAllowList(aAllowList);
  return NS_OK;
}

NS_IMETHODIMP
URLQueryStringStripper::OnStripOnShareUpdate(const nsTArray<nsString>& aArgs,
                                             JSContext* aCx) {
  for (const auto& ruleString : aArgs) {
    dom::StripRule rule;
    if (NS_WARN_IF(!rule.Init(ruleString))) {
      // Skipping malformed rules
      continue;
    }
    for (const auto& topLevelSite : rule.mTopLevelSites) {
      mStripOnShareMap.InsertOrUpdate(NS_ConvertUTF16toUTF8(topLevelSite),
                                      rule);
    }
  }
  return NS_OK;
}
// static
NS_IMETHODIMP
URLQueryStringStripper::TestGetStripList(nsACString& aStripList) {
  aStripList.Truncate();

  StringJoinAppend(aStripList, " "_ns, mList,
                   [](auto& aResult, const auto& aValue) {
                     aResult.Append(NS_ConvertUTF16toUTF8(aValue));
                   });
  return NS_OK;
}

/* nsIObserver */
NS_IMETHODIMP
URLQueryStringStripper::Observe(nsISupports*, const char* aTopic,
                                const char16_t*) {
  // Since this class is created at profile-after-change by the Category
  // Manager, it's expected to implement nsIObserver; however, we have nothing
  // interesting to do here.
  MOZ_ASSERT(strcmp(aTopic, "profile-after-change") == 0);

  return NS_OK;
}

}  // namespace mozilla
