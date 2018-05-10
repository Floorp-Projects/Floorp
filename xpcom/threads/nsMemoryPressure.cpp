/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemoryPressure.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"

#include "nsThreadUtils.h"

using namespace mozilla;

static Atomic<int32_t, Relaxed> sMemoryPressurePending;
static_assert(MemPressure_None == 0,
              "Bad static initialization with the default constructor.");

MemoryPressureState
NS_GetPendingMemoryPressure()
{
  int32_t value = sMemoryPressurePending.exchange(MemPressure_None);
  return MemoryPressureState(value);
}

void
NS_DispatchEventualMemoryPressure(MemoryPressureState aState)
{
  /*
   * A new memory pressure event erases an ongoing (or stop of) memory pressure,
   * but an existing "new" memory pressure event takes precedence over a new
   * "ongoing" or "stop" memory pressure event.
   */
  switch (aState) {
    case MemPressure_None:
      sMemoryPressurePending = MemPressure_None;
      break;
    case MemPressure_New:
      sMemoryPressurePending = MemPressure_New;
      break;
    case MemPressure_Ongoing:
    case MemPressure_Stopping:
      sMemoryPressurePending.compareExchange(MemPressure_None,
                                             aState);
      break;
  }
}

nsresult
NS_DispatchMemoryPressure(MemoryPressureState aState)
{
  NS_DispatchEventualMemoryPressure(aState);
  nsCOMPtr<nsIRunnable> event = new Runnable("NS_DispatchEventualMemoryPressure");
  return NS_DispatchToMainThread(event);
}
