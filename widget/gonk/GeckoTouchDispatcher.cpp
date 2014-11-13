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
#include "mozilla/MouseEvents.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/Touch.h"
#include "nsAppShell.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsWindow.h"
#include <sys/types.h>
#include <unistd.h>
#include <utils/Timers.h>

#define LOG(args...)                                            \
  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)

// uncomment to print log resample data
// #define LOG_RESAMPLE_DATA 1

namespace mozilla {

// Amount of time in MS before an input is considered expired.
static const uint64_t kInputExpirationThresholdMs = 1000;

static StaticRefPtr<GeckoTouchDispatcher> sTouchDispatcher;

GeckoTouchDispatcher::GeckoTouchDispatcher()
  : mTouchQueueLock("GeckoTouchDispatcher::mTouchQueueLock")
  , mTouchEventsFiltered(false)
  , mTouchTimeDiff(0)
  , mLastTouchTime(TimeStamp::Now())
{
  // Since GeckoTouchDispatcher is initialized when input is initialized
  // and reads gfxPrefs, it is the first thing to touch gfxPrefs.
  // The first thing to touch gfxPrefs MUST occur on the main thread and init
  // the singleton
  MOZ_ASSERT(sTouchDispatcher == nullptr);
  MOZ_ASSERT(NS_IsMainThread());
  gfxPrefs::GetSingleton();

  mEnabledUniformityInfo = gfxPrefs::UniformityInfo();
  mResamplingEnabled = gfxPrefs::TouchResampling() &&
                       gfxPrefs::HardwareVsyncEnabled();
  mVsyncAdjust = TimeDuration::FromMilliseconds(gfxPrefs::TouchVsyncSampleAdjust());
  mMaxPredict = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMaxPredict());
  mMinResampleTime = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleMinTime());
  mDelayedVsyncThreshold = TimeDuration::FromMilliseconds(gfxPrefs::TouchResampleVsyncDelayThreshold());
  sTouchDispatcher = this;
  ClearOnShutdown(&sTouchDispatcher);
}

class DispatchTouchEventsMainThread : public nsRunnable
{
public:
  DispatchTouchEventsMainThread(GeckoTouchDispatcher* aTouchDispatcher,
                                TimeStamp aVsyncTime)
    : mTouchDispatcher(aTouchDispatcher)
    , mVsyncTime(aVsyncTime)
  {
  }

  NS_IMETHOD Run()
  {
    mTouchDispatcher->DispatchTouchMoveEvents(mVsyncTime);
    return NS_OK;
  }

private:
  nsRefPtr<GeckoTouchDispatcher> mTouchDispatcher;
  TimeStamp mVsyncTime;
};

class DispatchSingleTouchMainThread : public nsRunnable
{
public:
  DispatchSingleTouchMainThread(GeckoTouchDispatcher* aTouchDispatcher,
                                MultiTouchInput& aTouch)
    : mTouchDispatcher(aTouchDispatcher)
    , mTouch(aTouch)
  {
  }

  NS_IMETHOD Run()
  {
    mTouchDispatcher->DispatchTouchEvent(mTouch);
    return NS_OK;
  }

private:
  nsRefPtr<GeckoTouchDispatcher> mTouchDispatcher;
  MultiTouchInput mTouch;
};

// Timestamp is in nanoseconds
/* static */ bool
GeckoTouchDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  if (sTouchDispatcher == nullptr) {
    return false;
  }

  MOZ_ASSERT(sTouchDispatcher->mResamplingEnabled);
  bool haveTouchData = false;
  {
    MutexAutoLock lock(sTouchDispatcher->mTouchQueueLock);
    haveTouchData = !sTouchDispatcher->mTouchMoveEvents.empty();
  }

  if (haveTouchData) {
    NS_DispatchToMainThread(new DispatchTouchEventsMainThread(sTouchDispatcher, aVsyncTimestamp));
  }

  return haveTouchData;
}

// Touch data timestamps are in milliseconds, aEventTime is in nanoseconds
void
GeckoTouchDispatcher::NotifyTouch(MultiTouchInput& aTouch, TimeStamp aEventTime)
{
  if (aTouch.mType == MultiTouchInput::MULTITOUCH_MOVE) {
    MutexAutoLock lock(mTouchQueueLock);
    if (mResamplingEnabled) {
      mTouchMoveEvents.push_back(aTouch);
      mTouchTimeDiff = aEventTime - mLastTouchTime;
      mLastTouchTime = aEventTime;
      return;
    }

    if (mTouchMoveEvents.empty()) {
      mTouchMoveEvents.push_back(aTouch);
    } else {
      // Coalesce touch move events
      mTouchMoveEvents.back() = aTouch;
    }

    NS_DispatchToMainThread(new DispatchTouchEventsMainThread(this, TimeStamp::Now()));
  } else {
    NS_DispatchToMainThread(new DispatchSingleTouchMainThread(this, aTouch));
  }
}

void
GeckoTouchDispatcher::DispatchTouchMoveEvents(TimeStamp aVsyncTime)
{
  MultiTouchInput touchMove;

  {
    MutexAutoLock lock(mTouchQueueLock);
    if (mTouchMoveEvents.empty()) {
      return;
    }

    if (mResamplingEnabled) {
      int touchCount = mTouchMoveEvents.size();
      // Both aVsynctime and mLastTouchTime are uint64_t
      // Need to store as a signed int.
      TimeDuration vsyncTouchDiff = aVsyncTime - mLastTouchTime;
      bool resample = (touchCount > 1) &&
        (vsyncTouchDiff > mMinResampleTime);
      // The delay threshold is a positive pref, but we're testing to see if the
      // vsync time is delayed from the touch, so add a negative sign.
      bool isDelayedVsyncEvent = vsyncTouchDiff < -mDelayedVsyncThreshold;

      if (!resample) {
        touchMove = mTouchMoveEvents.back();
        mTouchMoveEvents.clear();
        if (!isDelayedVsyncEvent) {
          mTouchMoveEvents.push_back(touchMove);
        }
      } else {
        ResampleTouchMoves(touchMove, aVsyncTime);
      }
    } else {
      touchMove = mTouchMoveEvents.back();
      mTouchMoveEvents.clear();
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
  int32_t id = aCurrentTouch.mIdentifier;
  for (size_t i = 0; i < aOtherTouch.mTouches.Length(); i++) {
    SingleTouchData& touch = aOtherTouch.mTouches[i];
    if (touch.mIdentifier == id) {
      return touch;
    }
  }

  // We can have situations where a previous touch event had 2 fingers
  // and we lift 1 finger off. In those cases, we won't find the touch event
  // with given id, so just return the current touch, which will be resampled
  // without modification and dispatched.
  return aCurrentTouch;
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
    const SingleTouchData& base = aBase.mTouches[i];
    const SingleTouchData& current = GetTouchByID(base, aCurrent);

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

  if (mLastTouchTime < sampleTime) {
    TimeDuration maxResampleTime = std::min(mTouchTimeDiff / 2, mMaxPredict);
    TimeStamp maxTimestamp = mLastTouchTime + maxResampleTime;
    if (sampleTime > maxTimestamp) {
      sampleTime = maxTimestamp;
      #ifdef LOG_RESAMPLE_DATA
      LOG("Overshot extrapolation time, adjusting sample time\n");
      #endif
    }
  }

  ResampleTouch(aOutTouch, baseTouch, currentTouch, sampleTime - (mLastTouchTime - mTouchTimeDiff), mTouchTimeDiff);

  // Both mTimeStamp and mTime are being updated to sampleTime here.
  // mTime needs to be updated using a delta since TimeStamp doesn't
  // provide a way to obtain a raw value.
  aOutTouch.mTime += (sampleTime - aOutTouch.mTimeStamp).ToMilliseconds();
  aOutTouch.mTimeStamp = sampleTime;
}

// Some touch events get sent as mouse events. If APZ doesn't capture the event
// and if a touch only has 1 touch input, we can send a mouse event.
void
GeckoTouchDispatcher::DispatchMouseEvent(MultiTouchInput& aMultiTouch,
                                         bool aForwardToChildren)
{
  WidgetMouseEvent mouseEvent = ToWidgetMouseEvent(aMultiTouch, nullptr);
  if (mouseEvent.message == NS_EVENT_NULL) {
    return;
  }

  mouseEvent.mFlags.mNoCrossProcessBoundaryForwarding = !aForwardToChildren;
  nsWindow::DispatchInputEvent(mouseEvent);
}

static bool
IsExpired(const MultiTouchInput& aTouch)
{
  // No pending events, the filter state can be updated.
  uint64_t timeNowMs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000000;
  return (timeNowMs - aTouch.mTime) > kInputExpirationThresholdMs;
}
void
GeckoTouchDispatcher::DispatchTouchEvent(MultiTouchInput& aMultiTouch)
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

  bool captured = false;
  WidgetTouchEvent event = aMultiTouch.ToWidgetTouchEvent(nullptr);
  nsEventStatus status = nsWindow::DispatchInputEvent(event, &captured);

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

  if (!captured && (aMultiTouch.mTouches.Length() == 1)) {
    bool forwardToChildren = status != nsEventStatus_eConsumeNoDefault;
    DispatchMouseEvent(aMultiTouch, forwardToChildren);
  }
}

WidgetMouseEvent
GeckoTouchDispatcher::ToWidgetMouseEvent(const MultiTouchInput& aMultiTouch,
                                         nsIWidget* aWidget) const
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(),
                    "Can only convert To WidgetMouseEvent on main thread");

  uint32_t mouseEventType = NS_EVENT_NULL;
  switch (aMultiTouch.mType) {
    case MultiTouchInput::MULTITOUCH_START:
      mouseEventType = NS_MOUSE_BUTTON_DOWN;
      break;
    case MultiTouchInput::MULTITOUCH_MOVE:
      mouseEventType = NS_MOUSE_MOVE;
      break;
    case MultiTouchInput::MULTITOUCH_CANCEL:
    case MultiTouchInput::MULTITOUCH_END:
      mouseEventType = NS_MOUSE_BUTTON_UP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetMouseEvent");
      break;
  }

  WidgetMouseEvent event(true, mouseEventType, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  const SingleTouchData& firstTouch = aMultiTouch.mTouches[0];
  event.refPoint.x = firstTouch.mScreenPoint.x;
  event.refPoint.y = firstTouch.mScreenPoint.y;

  event.time = aMultiTouch.mTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.modifiers = aMultiTouch.modifiers;

  if (mouseEventType != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  return event;
}

} // namespace mozilla
