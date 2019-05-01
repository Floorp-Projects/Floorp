/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SwipeTracker_h
#define SwipeTracker_h

#include "EventForwards.h"
#include "mozilla/layers/AxisPhysicsMSDModel.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "Units.h"

class nsChildView;
class nsIWidget;

namespace mozilla {

class PanGestureInput;

/**
 * SwipeTracker turns PanGestureInput events into swipe events
 * (WidgetSimpleGestureEvent) and dispatches them into Gecko.
 * The swiping behavior mirrors the behavior of the Cocoa API
 * -[NSEvent
 *     trackSwipeEventWithOptions:dampenAmountThresholdMin:max:usingHandler:].
 * The advantage of using this class over the Cocoa API is that this class
 * properly supports submitting queued up events to it, and that it hopefully
 * doesn't intermittently break scrolling the way the Cocoa API does (bug
 * 927702).
 *
 * The swipe direction is either left or right. It is determined before the
 * SwipeTracker is created and stays fixed during the swipe.
 * During the swipe, the swipe has a current "value" which is between 0 and the
 * target value. The target value is either 1 (swiping left) or -1 (swiping
 * right) - see SwipeSuccessTargetValue().
 * A swipe can either succeed or fail. If it succeeds, the swipe animation
 * animates towards the success target value; if it fails, it animates back to
 * a value of 0. A swipe can only succeed if the user is swiping in an allowed
 * direction. (Since both the allowed directions and the swipe direction are
 * known at swipe start time, it's clear from the beginning whether a swipe is
 * doomed to fail. In that case, the purpose of the SwipeTracker is to simulate
 * a bounce-back animation.)
 */
class SwipeTracker final : public nsARefreshObserver {
 public:
  NS_INLINE_DECL_REFCOUNTING(SwipeTracker, override)

  SwipeTracker(nsChildView& aWidget, const PanGestureInput& aSwipeStartEvent,
               uint32_t aAllowedDirections, uint32_t aSwipeDirection);

  void Destroy();

  nsEventStatus ProcessEvent(const PanGestureInput& aEvent);
  void CancelSwipe(const TimeStamp& aTimeStamp);

  static WidgetSimpleGestureEvent CreateSwipeGestureEvent(
      EventMessage aMsg, nsIWidget* aWidget,
      const LayoutDeviceIntPoint& aPosition, const TimeStamp& aTimeStamp);

  // nsARefreshObserver
  void WillRefresh(mozilla::TimeStamp aTime) override;

 protected:
  ~SwipeTracker();

  bool SwipingInAllowedDirection() const {
    return mAllowedDirections & mSwipeDirection;
  }
  double SwipeSuccessTargetValue() const;
  double ClampToAllowedRange(double aGestureAmount) const;
  bool ComputeSwipeSuccess() const;
  void StartAnimating(double aTargetValue);
  void SwipeFinished(const TimeStamp& aTimeStamp);
  void UnregisterFromRefreshDriver();
  bool SendSwipeEvent(EventMessage aMsg, uint32_t aDirection, double aDelta,
                      const TimeStamp& aTimeStamp);

  nsChildView& mWidget;
  RefPtr<nsRefreshDriver> mRefreshDriver;
  layers::AxisPhysicsMSDModel mAxis;
  const LayoutDeviceIntPoint mEventPosition;
  TimeStamp mLastEventTimeStamp;
  TimeStamp mLastAnimationFrameTime;
  const uint32_t mAllowedDirections;
  const uint32_t mSwipeDirection;
  double mGestureAmount;
  double mCurrentVelocity;
  bool mEventsAreControllingSwipe;
  bool mEventsHaveStartedNewGesture;
  bool mRegisteredWithRefreshDriver;
};

}  // namespace mozilla

#endif  // SwipeTracker_h
