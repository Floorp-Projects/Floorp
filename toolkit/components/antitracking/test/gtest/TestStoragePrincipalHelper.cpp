/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsICookieJarSettings.h"
#include "nsILoadInfo.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"

#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StoragePrincipalHelper.h"

using mozilla::Preferences;
using namespace mozilla;

/**
 * Creates a test channel with CookieJarSettings which have a partitionKey set.
 */
nsresult CreateMockChannel(nsIPrincipal* aPrincipal, bool isThirdParty,
                           const nsACString& aPartitionKey,
                           nsIChannel** aChannel,
                           nsICookieJarSettings** aCookieJarSettings) {
  nsCOMPtr<nsIURI> mockUri;
  nsresult rv = NS_NewURI(getter_AddRefs(mockUri), "http://example.com"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> mockChannel;
  nsCOMPtr<nsIIOService> service = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = service->NewChannelFromURI(mockUri, nullptr, aPrincipal, aPrincipal, 0,
                                  nsContentPolicyType::TYPE_OTHER,
                                  getter_AddRefs(mockChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadInfo> mockLoadInfo = mockChannel->LoadInfo();
  rv = mockLoadInfo->SetIsInThirdPartyContext(isThirdParty);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICookieJarSettings> cjs;
  rv = mockLoadInfo->GetCookieJarSettings(getter_AddRefs(cjs));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> partitionKeyUri;
  rv = NS_NewURI(getter_AddRefs(partitionKeyUri), aPartitionKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cjs->InitWithURI(partitionKeyUri, false);
  NS_ENSURE_SUCCESS(rv, rv);

  cjs.forget(aCookieJarSettings);
  mockChannel.forget(aChannel);
  return NS_OK;
}

TEST(TestStoragePrincipalHelper, TestCreateContentPrincipal)
{
  nsCOMPtr<nsIPrincipal> contentPrincipal =
      BasePrincipal::CreateContentPrincipal("https://example.com"_ns);
  EXPECT_TRUE(contentPrincipal);

  nsCOMPtr<nsIChannel> mockChannel;
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv = CreateMockChannel(
      contentPrincipal, false, "https://example.org"_ns,
      getter_AddRefs(mockChannel), getter_AddRefs(cookieJarSettings));
  ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";

  nsCOMPtr<nsIPrincipal> storagePrincipal;
  rv = StoragePrincipalHelper::Create(mockChannel, contentPrincipal, true,
                                      getter_AddRefs(storagePrincipal));
  ASSERT_EQ(rv, NS_OK) << "Should not fail for ContentPrincipal";
  EXPECT_TRUE(storagePrincipal);

  nsCOMPtr<nsIPrincipal> storagePrincipalSW;
  rv = StoragePrincipalHelper::CreatePartitionedPrincipalForServiceWorker(
      contentPrincipal, cookieJarSettings, getter_AddRefs(storagePrincipalSW));
  ASSERT_EQ(rv, NS_OK) << "Should not fail for ContentPrincipal";
  EXPECT_TRUE(storagePrincipalSW);
}

TEST(TestStoragePrincipalHelper, TestCreateNullPrincipal)
{
  RefPtr<NullPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  EXPECT_TRUE(nullPrincipal);

  nsCOMPtr<nsIChannel> mockChannel;
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  nsresult rv = CreateMockChannel(
      nullPrincipal, false, "https://example.org"_ns,
      getter_AddRefs(mockChannel), getter_AddRefs(cookieJarSettings));
  ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";

  nsCOMPtr<nsIPrincipal> storagePrincipal;
  rv = StoragePrincipalHelper::Create(mockChannel, nullPrincipal, true,
                                      getter_AddRefs(storagePrincipal));
  EXPECT_NS_FAILED(rv) << "Should fail for NullPrincipal";
  EXPECT_FALSE(storagePrincipal);

  nsCOMPtr<nsIPrincipal> storagePrincipalSW;
  rv = StoragePrincipalHelper::CreatePartitionedPrincipalForServiceWorker(
      nullPrincipal, cookieJarSettings, getter_AddRefs(storagePrincipalSW));
  EXPECT_NS_FAILED(rv) << "Should fail for NullPrincipal";
  EXPECT_FALSE(storagePrincipal);
}

TEST(TestStoragePrincipalHelper, TestGetPrincipalCookieBehavior4)
{
  Preferences::SetInt("network.cookie.cookieBehavior", 4);

  nsCOMPtr<nsIPrincipal> contentPrincipal =
      BasePrincipal::CreateContentPrincipal("https://example.com"_ns);
  EXPECT_TRUE(contentPrincipal);

  for (auto isThirdParty : {false, true}) {
    nsCOMPtr<nsIChannel> mockChannel;
    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    nsresult rv = CreateMockChannel(
        contentPrincipal, isThirdParty, "https://example.org"_ns,
        getter_AddRefs(mockChannel), getter_AddRefs(cookieJarSettings));
    ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";

    nsCOMPtr<nsIPrincipal> testPrincipal;
    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::eRegularPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get regular principal";
    EXPECT_TRUE(testPrincipal);
    EXPECT_TRUE(testPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty());

    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::ePartitionedPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get partitioned principal";
    EXPECT_TRUE(testPrincipal);
    EXPECT_TRUE(
        testPrincipal->OriginAttributesRef().mPartitionKey.EqualsLiteral(
            "(https,example.org)"));

    // We should always get regular principal if the dFPI is disabled.
    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::eForeignPartitionedPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get foreign partitioned principal";
    EXPECT_TRUE(testPrincipal);
    EXPECT_TRUE(testPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty());

    // Note that we don't test eStorageAccessPrincipal here because it's hard to
    // setup the right state for the storage access in gTest.
  }
}

TEST(TestStoragePrincipalHelper, TestGetPrincipalCookieBehavior5)
{
  Preferences::SetInt("network.cookie.cookieBehavior", 5);

  nsCOMPtr<nsIPrincipal> contentPrincipal =
      BasePrincipal::CreateContentPrincipal("https://example.com"_ns);
  EXPECT_TRUE(contentPrincipal);

  for (auto isThirdParty : {false, true}) {
    nsCOMPtr<nsIChannel> mockChannel;
    nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
    nsresult rv = CreateMockChannel(
        contentPrincipal, isThirdParty, "https://example.org"_ns,
        getter_AddRefs(mockChannel), getter_AddRefs(cookieJarSettings));
    ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";

    nsCOMPtr<nsIPrincipal> testPrincipal;
    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::eRegularPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get regular principal";
    EXPECT_TRUE(testPrincipal);
    EXPECT_TRUE(testPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty());

    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::ePartitionedPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get partitioned principal";
    EXPECT_TRUE(testPrincipal);
    EXPECT_TRUE(
        testPrincipal->OriginAttributesRef().mPartitionKey.EqualsLiteral(
            "(https,example.org)"));

    // We should always get regular principal if the dFPI is disabled.
    rv = StoragePrincipalHelper::GetPrincipal(
        mockChannel, StoragePrincipalHelper::eForeignPartitionedPrincipal,
        getter_AddRefs(testPrincipal));
    ASSERT_EQ(rv, NS_OK) << "Could not get foreign partitioned principal";
    EXPECT_TRUE(testPrincipal);
    if (isThirdParty) {
      EXPECT_TRUE(
          testPrincipal->OriginAttributesRef().mPartitionKey.EqualsLiteral(
              "(https,example.org)"));
    } else {
      EXPECT_TRUE(testPrincipal->OriginAttributesRef().mPartitionKey.IsEmpty());
    }

    // Note that we don't test eStorageAccessPrincipal here because it's hard to
    // setup the right state for the storage access in gTest.
  }
}
