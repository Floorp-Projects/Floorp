/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LinkServiceCommon.h"

#include "mozilla/Maybe.h"
#include "mozilla/SHA1.h"
#include "mozilla/TimeStamp.h"
#include "nsID.h"

using namespace mozilla;

void SeedNetworkId(SHA1Sum& aSha1) {
  static Maybe<nsID> seed = ([]() {
    Maybe<nsID> uuid(std::in_place);
    if (NS_FAILED(nsID::GenerateUUIDInPlace(*uuid))) {
      uuid.reset();
    }
    return uuid;
  })();
  if (seed) {
    aSha1.update(seed.ptr(), sizeof(*seed));
  } else {
    TimeStamp timestamp = TimeStamp::ProcessCreation();
    aSha1.update(&timestamp, sizeof(timestamp));
  }
}
