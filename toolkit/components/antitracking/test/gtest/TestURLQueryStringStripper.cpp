/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"

#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/URLQueryStringStripper.h"

using namespace mozilla;

static const char kPrefQueryStrippingEnabled[] =
    "privacy.query_stripping.enabled";
static const char kPrefQueryStrippingEnabledPBM[] =
    "privacy.query_stripping.enabled.pbmode";
static const char kPrefQueryStrippingList[] =
    "privacy.query_stripping.strip_list";

void DoTest(const nsACString& aTestURL, const bool aIsPBM,
            const nsACString& aExpectedURL, uint32_t aExpectedResult) {
  nsCOMPtr<nsIURI> testURI;

  NS_NewURI(getter_AddRefs(testURI), aTestURL);

  nsCOMPtr<nsIURI> strippedURI;
  uint32_t numStripped =
      URLQueryStringStripper::Strip(testURI, aIsPBM, strippedURI);

  EXPECT_TRUE(numStripped == aExpectedResult);

  if (!numStripped) {
    EXPECT_TRUE(!strippedURI);
  } else {
    EXPECT_TRUE(strippedURI->GetSpecOrDefault().Equals(aExpectedURL));
  }
}

class StripListObserver final : public nsIURLQueryStrippingListObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLQUERYSTRIPPINGLISTOBSERVER

  bool IsStillWaiting() { return mWaitingObserver; }
  void StartWaitingObserver() { mWaitingObserver = true; }

  StripListObserver() : mWaitingObserver(false) {
    mService = do_GetService("@mozilla.org/query-stripping-list-service;1");
    mService->RegisterAndRunObserver(this);
  }

 private:
  ~StripListObserver() {
    mService->UnregisterObserver(this);
    mService = nullptr;
  }

  bool mWaitingObserver;
  nsCOMPtr<nsIURLQueryStrippingListService> mService;
};

NS_IMPL_ISUPPORTS(StripListObserver, nsIURLQueryStrippingListObserver)

NS_IMETHODIMP
StripListObserver::OnQueryStrippingListUpdate(const nsAString& aStripList,
                                              const nsACString& aAllowList) {
  mWaitingObserver = false;
  return NS_OK;
}

TEST(TestURLQueryStringStripper, TestPrefDisabled)
{
  // Disable the query string stripping by the pref and make sure the stripping
  // is disabled.
  // Note that we don't need to run a dummy test to create the
  // URLQueryStringStripper here because the stripper will never be created if
  // the query stripping is disabled.
  Preferences::SetCString(kPrefQueryStrippingList, "fooBar foobaz");
  Preferences::SetBool(kPrefQueryStrippingEnabled, false);
  Preferences::SetBool(kPrefQueryStrippingEnabledPBM, false);

  for (bool isPBM : {false, true}) {
    DoTest("https://example.com/"_ns, isPBM, ""_ns, 0);
    DoTest("https://example.com/?Barfoo=123"_ns, isPBM, ""_ns, 0);
    DoTest("https://example.com/?fooBar=123&foobaz"_ns, isPBM, ""_ns, 0);
  }
}

TEST(TestURLQueryStringStripper, TestEmptyStripList)
{
  // Make sure there is no error if the strip list is empty.
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);
  Preferences::SetBool(kPrefQueryStrippingEnabledPBM, true);

  // To create the URLQueryStringStripper, we need to run a dummy test after
  // the query stripping is enabled. By doing this, the stripper will be
  // initiated and we are good to test.
  DoTest("https://example.com/"_ns, false, ""_ns, 0);

  // Set the strip list to empty and wait until the pref setting is set to the
  // stripper.
  RefPtr<StripListObserver> observer = new StripListObserver();
  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestURLQueryStringStripper, TestEmptyStripList)"_ns,
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  for (bool isPBM : {false, true}) {
    DoTest("https://example.com/"_ns, isPBM, ""_ns, 0);
    DoTest("https://example.com/?Barfoo=123"_ns, isPBM, ""_ns, 0);
    DoTest("https://example.com/?fooBar=123&foobaz"_ns, isPBM, ""_ns, 0);
  }
}

TEST(TestURLQueryStringStripper, TestStripping)
{
  // A dummy test to initiate the URLQueryStringStripper.
  DoTest("https://example.com/"_ns, false, ""_ns, 0);

  // Set the pref and create an observer to wait the pref setting is set to the
  // stripper.
  RefPtr<StripListObserver> observer = new StripListObserver();
  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "fooBar foobaz");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestURLQueryStringStripper, TestStripping)"_ns,
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  // Test the stripping.

  // Test all pref combinations.
  for (bool pref : {false, true}) {
    for (bool prefPBM : {false, true}) {
      Preferences::SetBool(kPrefQueryStrippingEnabled, pref);
      Preferences::SetBool(kPrefQueryStrippingEnabledPBM, prefPBM);

      // Test with normal and private browsing mode.
      for (bool isPBM : {false, true}) {
        bool expectStrip = (prefPBM && isPBM) || (pref && !isPBM);

        DoTest("https://example.com/"_ns, isPBM, ""_ns, 0);
        DoTest("https://example.com/?Barfoo=123"_ns, isPBM, ""_ns, 0);

        DoTest("https://example.com/?fooBar=123"_ns, isPBM,
               "https://example.com/"_ns, expectStrip ? 1 : 0);
        DoTest("https://example.com/?fooBar=123&foobaz"_ns, isPBM,
               "https://example.com/"_ns, expectStrip ? 2 : 0);
        DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz"_ns, isPBM,
               "https://example.com/?Barfoo=456"_ns, expectStrip ? 2 : 0);

        DoTest("https://example.com/?FOOBAR=123"_ns, isPBM,
               "https://example.com/"_ns, expectStrip ? 1 : 0);
        DoTest("https://example.com/?barfoo=foobar"_ns, isPBM,
               "https://example.com/?barfoo=foobar"_ns, 0);
        DoTest("https://example.com/?foobar=123&nostrip=456&FooBar=789"_ns,
               isPBM, "https://example.com/?nostrip=456"_ns,
               expectStrip ? 2 : 0);
        DoTest("https://example.com/?AfoobazB=123"_ns, isPBM,
               "https://example.com/?AfoobazB=123"_ns, 0);
      }
    }
  }

  // Change the strip list pref to see if it is updated properly.
  // We test this in normal browsing, so set the prefs accordingly.
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);
  Preferences::SetBool(kPrefQueryStrippingEnabledPBM, false);

  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "Barfoo bazfoo");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      "TEST(TestURLQueryStringStripper, TestStripping)"_ns,
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  DoTest("https://example.com/?fooBar=123"_ns, false, ""_ns, 0);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, false, ""_ns, 0);

  DoTest("https://example.com/?bazfoo=123"_ns, false, "https://example.com/"_ns,
         1);
  DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz=abc"_ns, false,
         "https://example.com/?fooBar=123&foobaz=abc"_ns, 1);
}
