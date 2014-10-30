/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* Copyright 2014 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GECKO_TOUCH_INPUT_DISPATCHER_h
#define GECKO_TOUCH_INPUT_DISPATCHER_h

#include "InputData.h"
#include "Units.h"
#include "mozilla/Mutex.h"
#include <vector>

class nsIWidget;

namespace mozilla {
class WidgetMouseEvent;

// Used to resample touch events whenever a vsync event occurs. It batches
// touch moves and on every vsync, resamples the touch position to create smooth
// scrolls. We use the Android touch resample algorithm. It uses a combination of
// extrapolation and interpolation. The algorithm takes the vsync time and
// subtracts mVsyncAdjust time in ms and creates a sample time. All touch events are
// relative to this sample time. If the last touch event occurs AFTER this
// sample time, interpolate the last two touch events. If the last touch event occurs BEFORE
// this sample time, we extrapolate the last two touch events to the sample
// time. The magic numbers defined as constants are taken from android
// InputTransport.cpp.
class GeckoTouchDispatcher
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GeckoTouchDispatcher)

public:
  GeckoTouchDispatcher();
  void NotifyTouch(MultiTouchInput& aTouch, TimeStamp aEventTime);
  void DispatchTouchEvent(MultiTouchInput& aMultiTouch);
  void DispatchTouchMoveEvents(TimeStamp aVsyncTime);
  static bool NotifyVsync(TimeStamp aVsyncTimestamp);

private:
  int32_t InterpolateTouch(MultiTouchInput& aOutTouch, TimeStamp aSampleTime);
  int32_t ExtrapolateTouch(MultiTouchInput& aOutTouch, TimeStamp aSampleTime);
  void ResampleTouchMoves(MultiTouchInput& aOutTouch, TimeStamp vsyncTime);
  void SendTouchEvent(MultiTouchInput& aData);
  void DispatchMouseEvent(MultiTouchInput& aMultiTouch,
                          bool aForwardToChildren);
  WidgetMouseEvent ToWidgetMouseEvent(const MultiTouchInput& aData, nsIWidget* aWidget) const;

  // mTouchQueueLock are used to protect the vector below
  // as it is accessed on the vsync thread and main thread
  Mutex mTouchQueueLock;
  std::vector<MultiTouchInput> mTouchMoveEvents;

  bool mResamplingEnabled;
  bool mTouchEventsFiltered;
  bool mEnabledUniformityInfo;

  // All times below are in nanoseconds
  TimeDuration mVsyncAdjust;     // Time from vsync we create sample times from
  TimeDuration mMaxPredict;      // How far into the future we're allowed to extrapolate

  // Amount of time between vsync and the last event that is required before we
  // resample
  TimeDuration mMinResampleTime;

  // The time difference between the last two touch move events
  TimeDuration mTouchTimeDiff;

  // The system time at which the last touch event occured
  TimeStamp mLastTouchTime;

  // Threshold if a vsync event runs too far behind touch events
  TimeDuration mDelayedVsyncThreshold;
};

} // namespace mozilla
#endif // GECKO_TOUCH_INPUT_DISPATCHER_h
