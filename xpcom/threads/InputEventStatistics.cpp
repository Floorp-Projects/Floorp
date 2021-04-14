/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputEventStatistics.h"

#include "mozilla/Preferences.h"
#include "nsRefreshDriver.h"

namespace mozilla {

/*static*/ InputEventStatistics& InputEventStatistics::Get() {
  static UniquePtr<InputEventStatistics> sInstance;
  if (!sInstance) {
    sInstance = MakeUnique<InputEventStatistics>(ConstructorCookie());
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

TimeDuration InputEventStatistics::TimeDurationCircularBuffer::GetMean() {
  return mTotal / (int64_t)mSize;
}

InputEventStatistics::InputEventStatistics(ConstructorCookie&&)
    : mEnable(false) {
  MOZ_ASSERT(Preferences::IsServiceAvailable());
  uint32_t inputDuration = Preferences::GetUint(
      "input_event_queue.default_duration_per_event", sDefaultInputDuration);

  TimeDuration defaultDuration = TimeDuration::FromMilliseconds(inputDuration);

  uint32_t count = Preferences::GetUint(
      "input_event_queue.count_for_prediction", sInputCountForPrediction);

  mLastInputDurations =
      MakeUnique<TimeDurationCircularBuffer>(count, defaultDuration);

  uint32_t maxDuration = Preferences::GetUint("input_event_queue.duration.max",
                                              sMaxReservedTimeForHandlingInput);

  uint32_t minDuration = Preferences::GetUint("input_event_queue.duration.min",
                                              sMinReservedTimeForHandlingInput);

  mMaxInputDuration = TimeDuration::FromMilliseconds(maxDuration);
  mMinInputDuration = TimeDuration::FromMilliseconds(minDuration);
}

TimeStamp InputEventStatistics::GetInputHandlingStartTime(
    uint32_t aInputCount) {
  MOZ_ASSERT(mEnable);
  Maybe<TimeStamp> nextTickHint = nsRefreshDriver::GetNextTickHint();

  if (nextTickHint.isNothing()) {
    // Return a past time to process input events immediately.
    return TimeStamp::Now() - TimeDuration::FromMilliseconds(1);
  }
  TimeDuration inputCost = mLastInputDurations->GetMean() * aInputCount;
  inputCost = inputCost > mMaxInputDuration   ? mMaxInputDuration
              : inputCost < mMinInputDuration ? mMinInputDuration
                                              : inputCost;

  return nextTickHint.value() - inputCost;
}

TimeDuration InputEventStatistics::GetMaxInputHandlingDuration() const {
  MOZ_ASSERT(StaticPrefs::dom_input_events_strict_input_vsync_alignment());
  return mMaxInputDuration;
}
}  // namespace mozilla
