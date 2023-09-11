/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "nsServiceManagerUtils.h"

#include "RelativeTimeline.h"

namespace mozilla {

int64_t RelativeTimeline::GetRandomTimelineSeed() {
  if (mRandomTimelineSeed == 0) {
    nsresult rv;
    nsCOMPtr<nsIRandomGenerator> randomGenerator =
        do_GetService("@mozilla.org/security/random-generator;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mRandomTimelineSeed = rand();
      return mRandomTimelineSeed;
    }

    uint8_t* buffer = nullptr;
    rv = randomGenerator->GenerateRandomBytes(sizeof(mRandomTimelineSeed),
                                              &buffer);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mRandomTimelineSeed = rand();
      return mRandomTimelineSeed;
    }

    memcpy(&mRandomTimelineSeed, buffer, sizeof(mRandomTimelineSeed));
    MOZ_ASSERT(buffer);
    free(buffer);
  }
  return mRandomTimelineSeed;
}

}  // namespace mozilla
