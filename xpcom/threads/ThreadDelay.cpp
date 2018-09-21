/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/ChaosMode.h"

#if defined(XP_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace mozilla {

void
DelayForChaosMode(ChaosFeature aFeature, const uint32_t aMicrosecondLimit)
{
  if (!ChaosMode::isActive(aFeature)) {
    return;
  }

  MOZ_ASSERT(aMicrosecondLimit <= 1000);
#if defined(XP_WIN)
  // Windows doesn't support sleeping at less than millisecond resolution.
  // We could spin here, or we could just sleep for one millisecond.
  // Sleeping for a full millisecond causes heavy delays, so we don't do anything
  // here for now until we have found a good way to sleep more precisely here.
#else
  const uint32_t duration = ChaosMode::randomUint32LessThan(aMicrosecondLimit);
  ::usleep(duration);
#endif
}

} // namespace mozilla
