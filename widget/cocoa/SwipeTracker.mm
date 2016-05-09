/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SwipeTracker.h"

#include "InputData.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "nsAlgorithm.h"
#include "nsChildView.h"
#include "UnitTransforms.h"

// These values were tweaked to make the physics feel similar to the native swipe.
static const double kSpringForce = 250.0;
static const double kVelocityTwitchTolerance = 0.0000001;
static const double kWholePagePixelSize = 1000.0;
static const double kRubberBandResistanceFactor = 4.0;
static const double kSwipeSuccessThreshold = 0.25;
static const double kSwipeSuccessVelocityContribution = 0.3;

namespace mozilla {

static already_AddRefed<nsRefreshDriver>
GetRefreshDriver(nsIWidget& aWidget)
{
  nsIWidgetListener* widgetListener = aWidget.GetWidgetListener();
  nsIPresShell* presShell = widgetListener ? widgetListener->GetPresShell() : nullptr;
  nsPresContext* presContext = presShell ? presShell->GetPresContext() : nullptr;
  RefPtr<nsRefreshDriver> refreshDriver = presContext ? presContext->RefreshDriver() : nullptr;
  return refreshDriver.forget();
}

SwipeTracker::SwipeTracker(nsChildView& aWidget,
                           const PanGestureInput& aSwipeStartEvent,
                           uint32_t aAllowedDirections,
                           uint32_t aSwipeDirection)
  : mWidget(aWidget)
  , mRefreshDriver(GetRefreshDriver(mWidget))
  , mAxis(0.0, 0.0, 0.0, kSpringForce, 1.0)
  , mEventPosition(RoundedToInt(ViewAs<LayoutDevicePixel>(aSwipeStartEvent.mPanStartPoint,
                                  PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent)))
  , mLastEventTimeStamp(aSwipeStartEvent.mTimeStamp)
  , mAllowedDirections(aAllowedDirections)
  , mSwipeDirection(aSwipeDirection)
  , mGestureAmount(0.0)
  , mCurrentVelocity(0.0)
  , mEventsAreControllingSwipe(true)
  , mEventsHaveStartedNewGesture(false)
  , mRegisteredWithRefreshDriver(false)
{
  SendSwipeEvent(eSwipeGestureStart, 0, 0.0);
  ProcessEvent(aSwipeStartEvent);
}

void
SwipeTracker::Destroy()
{
  UnregisterFromRefreshDriver();
}

SwipeTracker::~SwipeTracker()
{
  MOZ_ASSERT(!mRegisteredWithRefreshDriver, "Destroy needs to be called before deallocating");
}

double
SwipeTracker::SwipeSuccessTargetValue() const
{
  return (mSwipeDirection == nsIDOMSimpleGestureEvent::DIRECTION_RIGHT) ? -1.0 : 1.0;
}

double
SwipeTracker::ClampToAllowedRange(double aGestureAmount) const
{
  // gestureAmount needs to stay between -1 and 0 when swiping right and
  // between 0 and 1 when swiping left.
  double min = (mSwipeDirection == nsIDOMSimpleGestureEvent::DIRECTION_RIGHT) ? -1.0 : 0.0;
  double max = (mSwipeDirection == nsIDOMSimpleGestureEvent::DIRECTION_LEFT) ? 1.0 : 0.0;
  return clamped(aGestureAmount, min, max);
}

bool
SwipeTracker::ComputeSwipeSuccess() const
{
  double targetValue = SwipeSuccessTargetValue();

  // If the fingers were moving away from the target direction when they were
  // lifted from the touchpad, abort the swipe.
  if (mCurrentVelocity * targetValue < -kVelocityTwitchTolerance) {
    return false;
  }

  return (mGestureAmount * targetValue +
          mCurrentVelocity * targetValue * kSwipeSuccessVelocityContribution) >= kSwipeSuccessThreshold;
}

nsEventStatus
SwipeTracker::ProcessEvent(const PanGestureInput& aEvent)
{
  // If the fingers have already been lifted, don't process this event for swiping.
  if (!mEventsAreControllingSwipe) {
    // Return nsEventStatus_eConsumeNoDefault for events from the swipe gesture
    // and nsEventStatus_eIgnore for events of subsequent scroll gestures.
    if (aEvent.mType == PanGestureInput::PANGESTURE_MAYSTART ||
        aEvent.mType == PanGestureInput::PANGESTURE_START) {
      mEventsHaveStartedNewGesture = true;
    }
    return mEventsHaveStartedNewGesture ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
  }

  double delta = -aEvent.mPanDisplacement.x / mWidget.BackingScaleFactor() / kWholePagePixelSize;
  if (!SwipingInAllowedDirection()) {
    delta /= kRubberBandResistanceFactor;
  }
  mGestureAmount = ClampToAllowedRange(mGestureAmount + delta);
  SendSwipeEvent(eSwipeGestureUpdate, 0, mGestureAmount);

  if (aEvent.mType != PanGestureInput::PANGESTURE_END) {
    double elapsedSeconds = std::max(0.008, (aEvent.mTimeStamp - mLastEventTimeStamp).ToSeconds());
    mCurrentVelocity = delta / elapsedSeconds;
    mLastEventTimeStamp = aEvent.mTimeStamp;
  } else {
    mEventsAreControllingSwipe = false;
    bool didSwipeSucceed = SwipingInAllowedDirection() && ComputeSwipeSuccess();
    double targetValue = 0.0;
    if (didSwipeSucceed) {
      SendSwipeEvent(eSwipeGesture, mSwipeDirection, 0.0);
      targetValue = SwipeSuccessTargetValue();
    }
    StartAnimating(targetValue);
  }

  return nsEventStatus_eConsumeNoDefault;
}

void
SwipeTracker::StartAnimating(double aTargetValue)
{
  mAxis.SetPosition(mGestureAmount);
  mAxis.SetDestination(aTargetValue);
  mAxis.SetVelocity(mCurrentVelocity);

  mLastAnimationFrameTime = TimeStamp::Now();

  // Add ourselves as a refresh driver observer. The refresh driver
  // will call WillRefresh for each animation frame until we
  // unregister ourselves.
  MOZ_ASSERT(!mRegisteredWithRefreshDriver);
  if (mRefreshDriver) {
    mRefreshDriver->AddRefreshObserver(this, Flush_Style);
    mRegisteredWithRefreshDriver = true;
  }
}

void
SwipeTracker::WillRefresh(mozilla::TimeStamp aTime)
{
  TimeStamp now = TimeStamp::Now();
  mAxis.Simulate(now - mLastAnimationFrameTime);
  mLastAnimationFrameTime = now;

  bool isFinished = mAxis.IsFinished(1.0 / kWholePagePixelSize);
  mGestureAmount = (isFinished ? mAxis.GetDestination() : mAxis.GetPosition());
  SendSwipeEvent(eSwipeGestureUpdate, 0, mGestureAmount);

  if (isFinished) {
    UnregisterFromRefreshDriver();
    SwipeFinished();
  }
}

void
SwipeTracker::CancelSwipe()
{
  SendSwipeEvent(eSwipeGestureEnd, 0, 0.0);
}

void SwipeTracker::SwipeFinished()
{
  SendSwipeEvent(eSwipeGestureEnd, 0, 0.0);
  mWidget.SwipeFinished();
}

void
SwipeTracker::UnregisterFromRefreshDriver()
{
  if (mRegisteredWithRefreshDriver) {
    MOZ_ASSERT(mRefreshDriver, "How were we able to register, then?");
    mRefreshDriver->RemoveRefreshObserver(this, Flush_Style);
  }
  mRegisteredWithRefreshDriver = false;
}

/* static */ WidgetSimpleGestureEvent
SwipeTracker::CreateSwipeGestureEvent(EventMessage aMsg, nsIWidget* aWidget,
                                      const LayoutDeviceIntPoint& aPosition)
{
  WidgetSimpleGestureEvent geckoEvent(true, aMsg, aWidget);
  geckoEvent.mModifiers = 0;
  geckoEvent.mTimeStamp = TimeStamp::Now();
  geckoEvent.mRefPoint = aPosition;
  geckoEvent.buttons = 0;
  return geckoEvent;
}

bool
SwipeTracker::SendSwipeEvent(EventMessage aMsg, uint32_t aDirection, double aDelta)
{
  WidgetSimpleGestureEvent geckoEvent =
    CreateSwipeGestureEvent(aMsg, &mWidget, mEventPosition);
  geckoEvent.mDirection = aDirection;
  geckoEvent.mDelta = aDelta;
  geckoEvent.mAllowedDirections = mAllowedDirections;
  return mWidget.DispatchWindowEvent(geckoEvent);
}

} // namespace mozilla
