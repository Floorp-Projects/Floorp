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

#include "mozilla/NullPrincipal.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StoragePrincipalHelper.h"

using namespace mozilla;

/**
 * Creates a test channel with CookieJarSettings which have a partitionKey set.
 */
nsresult CreateMockChannel(nsIPrincipal* aPrincipal, nsIChannel** aChannel,
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
  nsCOMPtr<nsICookieJarSettings> cjs;
  rv = mockLoadInfo->GetCookieJarSettings(getter_AddRefs(cjs));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> partitionKeyUri;
  rv = NS_NewURI(getter_AddRefs(partitionKeyUri), "http://example.com"_ns);
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
  nsresult rv = CreateMockChannel(contentPrincipal, getter_AddRefs(mockChannel),
                                  getter_AddRefs(cookieJarSettings));
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
  nsresult rv = CreateMockChannel(nullPrincipal, getter_AddRefs(mockChannel),
                                  getter_AddRefs(cookieJarSettings));
  ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";

  nsCOMPtr<nsIPrincipal> storagePrincipal;
  rv = StoragePrincipalHelper::Create(mockChannel, nullPrincipal, true,
                                      getter_AddRefs(storagePrincipal));
  EXPECT_TRUE(NS_FAILED(rv)) << "Should fail for NullPrincipal";
  EXPECT_FALSE(storagePrincipal);

  nsCOMPtr<nsIPrincipal> storagePrincipalSW;
  rv = StoragePrincipalHelper::CreatePartitionedPrincipalForServiceWorker(
      nullPrincipal, cookieJarSettings, getter_AddRefs(storagePrincipalSW));
  EXPECT_TRUE(NS_FAILED(rv)) << "Should fail for NullPrincipal";
  EXPECT_FALSE(storagePrincipal);
}
