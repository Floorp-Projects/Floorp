/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIdleServiceOS2.h"
#include <algorithm>

// prototype for funtion imported from DSSaver
static int (*_System DSSaver_GetInactivityTime)(ULONG *, ULONG *);
#define SSCORE_NOERROR 0 // as in the DSSaver header files

NS_IMPL_ISUPPORTS_INHERITED0(nsIdleServiceOS2, nsIdleService)

nsIdleServiceOS2::nsIdleServiceOS2()
  : mHMod(NULLHANDLE), mInitialized(false)
{
  const char error[256] = "";
  if (DosLoadModule(error, 256, "SSCORE", &mHMod) == NO_ERROR) {
    if (DosQueryProcAddr(mHMod, 0, "SSCore_GetInactivityTime",
                         (PFN*)&DSSaver_GetInactivityTime) == NO_ERROR) {
      mInitialized = true;
    }
  }
}

nsIdleServiceOS2::~nsIdleServiceOS2()
{
  if (mHMod != NULLHANDLE) {
    DosFreeModule(mHMod);
  }
}

bool
nsIdleServiceOS2::PollIdleTime(uint32_t *aIdleTime)
{
  if (!mInitialized)
    return false;

  ULONG mouse, keyboard;
  if (DSSaver_GetInactivityTime(&mouse, &keyboard) != SSCORE_NOERROR) {
    return false;
  }

  // we are only interested in activity in general, so take the minimum
  // of both timers
  *aIdleTime = std::min(mouse, keyboard);
  return true;
}

bool
nsIdleServiceOS2::UsePollMode()
{
  return mInitialized;
}

