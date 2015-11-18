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

#include "FrameMetrics.h"
#include "GeckoProfiler.h"
#include "GeckoTouchDispatcher.h"
#include "InputData.h"
#include "ProfilerMarkers.h"
#include "base/basictypes.h"
#include "gfxPrefs.h"
#include "libui/Input.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorParent.h"
#include "nsAppShell.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsWindow.h"
#include <sys/types.h>
#include <unistd.h>
#include <utils/Timers.h>

#undef LOG
#define LOG(args...)                                            \
  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)

// uncomment to print log resample data
// #define LOG_RESAMPLE_DATA 1

namespace mozilla {

// Amount of time in MS before an input is considered expired.
static const uint64_t kInputExpirationThresholdMs = 1000;

static StaticRefPtr<GeckoTouchDispatcher> sTouchDispatcher;

/* static */ GeckoTouchDispatcher*
GeckoTouchDispatcher::GetInstance()
{
  if (!sTouchDispatcher) {
    sTouchDispatcher = new GeckoTouchDispatcher();
    ClearOnShutdown(&sTouchDispatcher);
  }
  return sTouchDispatcher;
}

GeckoTouchDispatcher::GeckoTouchDispatcher()
  : mTouchQueueLock("GeckoTouchDispatcher::mTouchQueueLock")
  , mHavePendingTouchMoves(false)
  , mInflightNonMoveEvents(0)
  , mTouchEventsFiltered(false)
{
  // Since GeckoTouchDispatcher is initialized when input is initialized
  // and reads gfxPrefs, it is the first thing to touch gfxPrefs.
  // The first thing to touch gfxPrefs MUST occur on the main thread and init
  // the singleton
  MOZ_ASSERT(sTouchDispatcher == nullptr);
  MOZ_ASSERT(NS_IsMainThread());
  gfxPrefs::GetSingleton();

  mEnabledUniformityInfo = gfxPrefs::UniformityInfo();
  mVsyncAdjust = TimeDuration::FromMilliseconds(gfxPrefs::TouchVsyncSampleAdjust());
  mMaxPredict = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMaxPredict());
  mMinDelta = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMinDelta());
  mOldTouchThreshold = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleOldTouchThreshold());
  mDelayedVsyncThreshold = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleVsyncDelayThreshold());
}

void
GeckoTouchDispatcher::SetCompositorVsyncScheduler(mozilla::layers::CompositorVsyncScheduler *aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  // We assume on b2g that there is only 1 CompositorParent
  MOZ_ASSERT(mCompositorVsyncScheduler == nullptr);
  mCompositorVsyncScheduler = aObserver;
}

void
GeckoTouchDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  layers::APZThreadUtils::AssertOnControllerThread();
  DispatchTouchMoveEvents(aVsyncTimestamp);
}

// Touch data timestamps are in milliseconds, aEventTime is in nanoseconds
void
GeckoTouchDispatcher::NotifyTouch(MultiTouchInput& aTouch, TimeStamp aEventTime)
{
  if (mCompositorVsyncScheduler) {
    mCompositorVsyncScheduler->SetNeedsComposite();
  }

  if (aTouch.mType == MultiTouchInput::MULTITOUCH_MOVE) {
    MutexAutoLock lock(mTouchQueueLock);
    if (mInflightNonMoveEvents > 0) {
      // If we have any pending non-move events, we shouldn't resample the
      // move events because we might end up dispatching events out of order.
      // Instead, fall back to a non-resampling in-order dispatch until we're
      // done processing the non-move events.
      layers::APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
        this, &GeckoTouchDispatcher::DispatchTouchEvent, aTouch));
      return;
    }

    mTouchMoveEvents.push_back(aTouch);
    mHavePendingTouchMoves = true;
  } else {
    { // scope lock
      MutexAutoLock lock(mTouchQueueLock);
      mInflightNonMoveEvents++;
    }
    layers::APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
      this, &GeckoTouchDispatcher::DispatchTouchNonMoveEvent, aTouch));
  }
}

void
GeckoTouchDispatcher::DispatchTouchNonMoveEvent(MultiTouchInput aInput)
{
  layers::APZThreadUtils::AssertOnControllerThread();

  // Flush pending touch move events, if there are any
  // (DispatchTouchMoveEvents will check the mHavePendingTouchMoves flag and
  // bail out if there's nothing to be done).
  NotifyVsync(TimeStamp::Now());
  DispatchTouchEvent(aInput);

  { // scope lock
    MutexAutoLock lock(mTouchQueueLock);
    mInflightNonMoveEvents--;
    MOZ_ASSERT(mInflightNonMoveEvents >= 0);
  }
}

void
GeckoTouchDispatcher::DispatchTouchMoveEvents(TimeStamp aVsyncTime)
{
  MultiTouchInput touchMove;

  {
    MutexAutoLock lock(mTouchQueueLock);
    if (!mHavePendingTouchMoves) {
      return;
    }
    mHavePendingTouchMoves = false;

    int touchCount = mTouchMoveEvents.size();
    TimeDuration vsyncTouchDiff = aVsyncTime - mTouchMoveEvents.back().mTimeStamp;
    // The delay threshold is a positive pref, but we're testing to see if the
    // vsync time is delayed from the touch, so add a negative sign.
    bool isDelayedVsyncEvent = vsyncTouchDiff < -mDelayedVsyncThreshold;
    bool isOldTouch = vsyncTouchDiff > mOldTouchThreshold;
    bool resample = (touchCount > 1) && !isDelayedVsyncEvent && !isOldTouch;

    if (!resample) {
      touchMove = mTouchMoveEvents.back();
      mTouchMoveEvents.clear();
      if (!isDelayedVsyncEvent && !isOldTouch) {
        mTouchMoveEvents.push_back(touchMove);
      }
    } else {
      ResampleTouchMoves(touchMove, aVsyncTime);
    }
  }

  DispatchTouchEvent(touchMove);
}

static int
Interpolate(int start, int end, TimeDuration aFrameDiff, TimeDuration aTouchDiff)
{
  return start + (((end - start) * aFrameDiff.ToMicroseconds()) / aTouchDiff.ToMicroseconds());
}

static const SingleTouchData&
GetTouchByID(const SingleTouchData& aCurrentTouch, MultiTouchInput& aOtherTouch)
{
  int32_t index = aOtherTouch.IndexOfTouch(aCurrentTouch.mIdentifier);
  if (index < 0) {
    // We can have situations where a previous touch event had 2 fingers
    // and we lift 1 finger off. In those cases, we won't find the touch event
    // with given id, so just return the current touch, which will be resampled
    // without modification and dispatched.
    return aCurrentTouch;
  }
  return aOtherTouch.mTouches[index];
}


// aTouchDiff is the duration between the base and current touch times
// aFrameDiff is the duration between the base and the time we're resampling to
static void
ResampleTouch(MultiTouchInput& aOutTouch,
              MultiTouchInput& aBase, MultiTouchInput& aCurrent,
              TimeDuration aFrameDiff, TimeDuration aTouchDiff)
{
  aOutTouch = aCurrent;

  // Make sure we only resample the correct finger.
  for (size_t i = 0; i < aOutTouch.mTouches.Length(); i++) {
    const SingleTouchData& current = aCurrent.mTouches[i];
    const SingleTouchData& base = GetTouchByID(current, aBase);

    const ScreenIntPoint& baseTouchPoint = base.mScreenPoint;
    const ScreenIntPoint& currentTouchPoint = current.mScreenPoint;

    ScreenIntPoint newSamplePoint;
    newSamplePoint.x = Interpolate(baseTouchPoint.x, currentTouchPoint.x, aFrameDiff, aTouchDiff);
    newSamplePoint.y = Interpolate(baseTouchPoint.y, currentTouchPoint.y, aFrameDiff, aTouchDiff);

    aOutTouch.mTouches[i].mScreenPoint = newSamplePoint;

#ifdef LOG_RESAMPLE_DATA
    const char* type = "extrapolate";
    if (aFrameDiff < aTouchDiff) {
      type = "interpolate";
    }

    float alpha = aFrameDiff / aTouchDiff;
    LOG("%s base (%d, %d), current (%d, %d) to (%d, %d) alpha %f, touch diff %d, frame diff %d\n",
        type,
        baseTouchPoint.x, baseTouchPoint.y,
        currentTouchPoint.x, currentTouchPoint.y,
        newSamplePoint.x, newSamplePoint.y,
        alpha, (int)aTouchDiff.ToMilliseconds(), (int)aFrameDiff.ToMilliseconds());
#endif
  }
}

/*
 * +> Base touch (The touch before current touch)
 * |
 * |     +> Current touch (Latest touch)
 * |     |
 * |     |      +> Maximum resample time
 * |     |      |
 * +-----+------+--------------------> Time
 *    ^      ^
 *    |      |
 *    +------+--> Potential vsync events which the touches are resampled to
 *    |      |
 *    |      +> Extrapolation
 *    |
 *    +> Interpolation
 */

void
GeckoTouchDispatcher::ResampleTouchMoves(MultiTouchInput& aOutTouch, TimeStamp aVsyncTime)
{
  MOZ_RELEASE_ASSERT(mTouchMoveEvents.size() >= 2);
  mTouchQueueLock.AssertCurrentThreadOwns();

  MultiTouchInput currentTouch = mTouchMoveEvents.back();
  mTouchMoveEvents.pop_back();
  MultiTouchInput baseTouch = mTouchMoveEvents.back();
  mTouchMoveEvents.clear();
  mTouchMoveEvents.push_back(currentTouch);

  TimeStamp sampleTime = aVsyncTime - mVsyncAdjust;
  TimeDuration touchDiff = currentTouch.mTimeStamp - baseTouch.mTimeStamp;

  if (touchDiff < mMinDelta) {
    aOutTouch = currentTouch;
    #ifdef LOG_RESAMPLE_DATA
    LOG("The touches are too close, skip resampling\n");
    #endif
    return;
  }

  if (currentTouch.mTimeStamp < sampleTime) {
    TimeDuration maxResampleTime = std::min(touchDiff / int64_t(2), mMaxPredict);
    TimeStamp maxTimestamp = currentTouch.mTimeStamp + maxResampleTime;
    if (sampleTime > maxTimestamp) {
      sampleTime = maxTimestamp;
      #ifdef LOG_RESAMPLE_DATA
      LOG("Overshot extrapolation time, adjusting sample time\n");
      #endif
    }
  }

  ResampleTouch(aOutTouch, baseTouch, currentTouch, sampleTime - baseTouch.mTimeStamp, touchDiff);

  // Both mTimeStamp and mTime are being updated to sampleTime here.
  // mTime needs to be updated using a delta since TimeStamp doesn't
  // provide a way to obtain a raw value.
  aOutTouch.mTime += (sampleTime - aOutTouch.mTimeStamp).ToMilliseconds();
  aOutTouch.mTimeStamp = sampleTime;
}

static bool
IsExpired(const MultiTouchInput& aTouch)
{
  // No pending events, the filter state can be updated.
  uint64_t timeNowMs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000000;
  return (timeNowMs - aTouch.mTime) > kInputExpirationThresholdMs;
}
void
GeckoTouchDispatcher::DispatchTouchEvent(MultiTouchInput aMultiTouch)
{
  if ((aMultiTouch.mType == MultiTouchInput::MULTITOUCH_END ||
       aMultiTouch.mType == MultiTouchInput::MULTITOUCH_CANCEL) &&
      aMultiTouch.mTouches.Length() == 1) {
    MutexAutoLock lock(mTouchQueueLock);
    mTouchMoveEvents.clear();
  } else if (aMultiTouch.mType == MultiTouchInput::MULTITOUCH_START &&
             aMultiTouch.mTouches.Length() == 1) {
    mTouchEventsFiltered = IsExpired(aMultiTouch);
  }

  if (mTouchEventsFiltered) {
    return;
  }

  nsWindow::DispatchTouchInput(aMultiTouch);

  if (mEnabledUniformityInfo && profiler_is_active()) {
    const char* touchAction = "Invalid";
    switch (aMultiTouch.mType) {
      case MultiTouchInput::MULTITOUCH_START:
        touchAction = "Touch_Event_Down";
        break;
      case MultiTouchInput::MULTITOUCH_MOVE:
        touchAction = "Touch_Event_Move";
        break;
      case MultiTouchInput::MULTITOUCH_END:
      case MultiTouchInput::MULTITOUCH_CANCEL:
        touchAction = "Touch_Event_Up";
        break;
    }

    const ScreenIntPoint& touchPoint = aMultiTouch.mTouches[0].mScreenPoint;
    TouchDataPayload* payload = new TouchDataPayload(touchPoint);
    PROFILER_MARKER_PAYLOAD(touchAction, payload);
  }
}

} // namespace mozilla
