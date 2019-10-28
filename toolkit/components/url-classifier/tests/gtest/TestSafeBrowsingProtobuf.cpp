/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "safebrowsing.pb.h"

#include "Common.h"

TEST(UrlClassifierProtobuf, Empty)
{
  using namespace mozilla::safebrowsing;

  const std::string CLIENT_ID = "firefox";

  // Construct a simple update request.
  FetchThreatListUpdatesRequest r;
  r.set_allocated_client(new ClientInfo());
  r.mutable_client()->set_client_id(CLIENT_ID);

  // Then serialize.
  std::string s;
  r.SerializeToString(&s);

  // De-serialize.
  FetchThreatListUpdatesRequest r2;
  r2.ParseFromString(s);

  ASSERT_EQ(r2.client().client_id(), CLIENT_ID);
}
