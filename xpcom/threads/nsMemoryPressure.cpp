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

const char* const kTopicMemoryPressure = "memory-pressure";
const char* const kTopicMemoryPressureStop = "memory-pressure-stop";
const char16_t* const kSubTopicLowMemoryNew = u"low-memory";
const char16_t* const kSubTopicLowMemoryOngoing = u"low-memory-ongoing";

// This is accessed from any thread through NS_NotifyOfEventualMemoryPressure
static Atomic<MemoryPressureState, Relaxed> sMemoryPressurePending(
    MemoryPressureState::NoPressure);

void NS_NotifyOfEventualMemoryPressure(MemoryPressureState aState) {
  MOZ_ASSERT(aState != MemoryPressureState::None);

  /*
   * A new memory pressure event erases an ongoing (or stop of) memory pressure,
   * but an existing "new" memory pressure event takes precedence over a new
   * "ongoing" or "stop" memory pressure event.
   */
  switch (aState) {
    case MemoryPressureState::None:
    case MemoryPressureState::LowMemory:
      sMemoryPressurePending = aState;
      break;
    case MemoryPressureState::NoPressure:
      sMemoryPressurePending.compareExchange(MemoryPressureState::None, aState);
      break;
  }
}

nsresult NS_NotifyOfMemoryPressure(MemoryPressureState aState) {
  NS_NotifyOfEventualMemoryPressure(aState);
  nsCOMPtr<nsIRunnable> event =
      new Runnable("NS_DispatchEventualMemoryPressure");
  return NS_DispatchToMainThread(event);
}

void NS_DispatchMemoryPressure() {
  MOZ_ASSERT(NS_IsMainThread());
  static MemoryPressureState sMemoryPressureStatus =
      MemoryPressureState::NoPressure;

  MemoryPressureState mpPending =
      sMemoryPressurePending.exchange(MemoryPressureState::None);
  if (mpPending == MemoryPressureState::None) {
    return;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (!os) {
    NS_WARNING("Can't get observer service!");
    return;
  }

  switch (mpPending) {
    case MemoryPressureState::None:
      MOZ_ASSERT_UNREACHABLE("Already handled this case above.");
      break;
    case MemoryPressureState::LowMemory:
      switch (sMemoryPressureStatus) {
        case MemoryPressureState::None:
          MOZ_ASSERT_UNREACHABLE("The internal status should never be None.");
          break;
        case MemoryPressureState::NoPressure:
          sMemoryPressureStatus = MemoryPressureState::LowMemory;
          os->NotifyObservers(nullptr, kTopicMemoryPressure,
                              kSubTopicLowMemoryNew);
          break;
        case MemoryPressureState::LowMemory:
          os->NotifyObservers(nullptr, kTopicMemoryPressure,
                              kSubTopicLowMemoryOngoing);
          break;
      }
      break;
    case MemoryPressureState::NoPressure:
      switch (sMemoryPressureStatus) {
        case MemoryPressureState::None:
          MOZ_ASSERT_UNREACHABLE("The internal status should never be None.");
          break;
        case MemoryPressureState::NoPressure:
          // Already no pressure.  Do nothing.
          break;
        case MemoryPressureState::LowMemory:
          sMemoryPressureStatus = MemoryPressureState::NoPressure;
          os->NotifyObservers(nullptr, kTopicMemoryPressureStop, nullptr);
          break;
      }
      break;
  }
}
