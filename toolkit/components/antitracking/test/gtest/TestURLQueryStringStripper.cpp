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
static const char kPrefQueryStrippingList[] =
    "privacy.query_stripping.strip_list";

void DoTest(const nsACString& aTestURL, const nsACString& aExpectedURL,
            bool aExpectedResult) {
  nsCOMPtr<nsIURI> testURI;

  NS_NewURI(getter_AddRefs(testURI), aTestURL);

  bool result;
  nsCOMPtr<nsIURI> strippedURI;
  result = URLQueryStringStripper::Strip(testURI, strippedURI);

  EXPECT_TRUE(result == aExpectedResult);

  if (!result) {
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

  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);
}

TEST(TestURLQueryStringStripper, TestEmptyStripList)
{
  // Make sure there is no error if the strip list is empty.
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);

  // To create the URLQueryStringStripper, we need to run a dummy test after
  // the query stripping is enabled. By doing this, the stripper will be
  // initiated and we are good to test.
  DoTest("https://example.com/"_ns, ""_ns, false);

  // Set the strip list to empty and wait until the pref setting is set to the
  // stripper.
  RefPtr<StripListObserver> observer = new StripListObserver();
  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);
}

TEST(TestURLQueryStringStripper, TestStripping)
{
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);

  // A dummy test to initiate the URLQueryStringStripper.
  DoTest("https://example.com/"_ns, ""_ns, false);

  // Set the pref and create an observer to wait the pref setting is set to the
  // stripper.
  RefPtr<StripListObserver> observer = new StripListObserver();
  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "fooBar foobaz");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  // Test the stripping.
  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);

  DoTest("https://example.com/?fooBar=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, "https://example.com/"_ns,
         true);
  DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz"_ns,
         "https://example.com/?Barfoo=456"_ns, true);

  DoTest("https://example.com/?FOOBAR=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?barfoo=foobar"_ns,
         "https://example.com/?barfoo=foobar"_ns, false);
  DoTest("https://example.com/?foobar=123&nostrip=456&FooBar=789"_ns,
         "https://example.com/?nostrip=456"_ns, true);
  DoTest("https://example.com/?AfoobazB=123"_ns,
         "https://example.com/?AfoobazB=123"_ns, false);

  // Change the strip list pref to see if it is updated properly.
  observer->StartWaitingObserver();
  Preferences::SetCString(kPrefQueryStrippingList, "Barfoo bazfoo");
  MOZ_ALWAYS_TRUE(mozilla::SpinEventLoopUntil(
      [&]() -> bool { return !observer->IsStillWaiting(); }));

  DoTest("https://example.com/?fooBar=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);

  DoTest("https://example.com/?bazfoo=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz=abc"_ns,
         "https://example.com/?fooBar=123&foobaz=abc"_ns, true);
}
