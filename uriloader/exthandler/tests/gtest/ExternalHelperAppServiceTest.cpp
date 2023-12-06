/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExternalHelperAppService.h"
#include "gtest/gtest.h"
#include "nsNetUtil.h"
#include "mozilla/StaticPrefs_network.h"

using namespace mozilla;

TEST(ExternalHelperAppService, EscapeURI)
{
  if (StaticPrefs::network_url_useDefaultURI()) {
    return;
  }

  nsCString input("myproto://hello world");
  nsCString expected("myproto://hello%20world");

  nsCOMPtr<nsIIOService> ios(do_GetIOService());
  nsCOMPtr<nsIURI> uri;
  nsresult rv = ios->NewURI(input, nullptr, nullptr, getter_AddRefs(uri));
  EXPECT_EQ(rv, NS_OK);

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  EXPECT_EQ(rv, NS_OK);

  // Constructing the URI does not escape the space character.
  ASSERT_TRUE(spec.Equals(input));

  // Created an escaped version of the URI.
  nsCOMPtr<nsIURI> escapedURI;
  rv = nsExternalHelperAppService::EscapeURI(uri, getter_AddRefs(escapedURI));
  EXPECT_EQ(rv, NS_OK);

  nsAutoCString escapedSpec;
  rv = escapedURI->GetSpec(escapedSpec);
  EXPECT_EQ(rv, NS_OK);

  // Escaped URI should have an escaped spec.
  ASSERT_TRUE(escapedSpec.Equals(expected));
}
