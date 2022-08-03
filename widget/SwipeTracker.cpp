/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SwipeTracker.h"

#include "InputData.h"
#include "mozilla/FlushType.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/dom/SimpleGestureEventBinding.h"
#include "nsAlgorithm.h"
#include "nsIWidget.h"
#include "nsRefreshDriver.h"
#include "UnitTransforms.h"

// These values were tweaked to make the physics feel similar to the native
// swipe.
static const double kSpringForce = 250.0;
static const double kSwipeSuccessThreshold = 0.25;

namespace mozilla {

static already_AddRefed<nsRefreshDriver> GetRefreshDriver(nsIWidget& aWidget) {
  nsIWidgetListener* widgetListener = aWidget.GetWidgetListener();
  PresShell* presShell =
      widgetListener ? widgetListener->GetPresShell() : nullptr;
  nsPresContext* presContext =
      presShell ? presShell->GetPresContext() : nullptr;
  RefPtr<nsRefreshDriver> refreshDriver =
      presContext ? presContext->RefreshDriver() : nullptr;
  return refreshDriver.forget();
}

SwipeTracker::SwipeTracker(nsIWidget& aWidget,
                           const PanGestureInput& aSwipeStartEvent,
                           uint32_t aAllowedDirections,
                           uint32_t aSwipeDirection)
    : mWidget(aWidget),
      mRefreshDriver(GetRefreshDriver(mWidget)),
      mAxis(0.0, 0.0, 0.0, kSpringForce, 1.0),
      mEventPosition(RoundedToInt(ViewAs<LayoutDevicePixel>(
          aSwipeStartEvent.mPanStartPoint,
          PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent))),
      mLastEventTimeStamp(aSwipeStartEvent.mTimeStamp),
      mAllowedDirections(aAllowedDirections),
      mSwipeDirection(aSwipeDirection),
      mGestureAmount(0.0),
      mCurrentVelocity(0.0),
      mEventsAreControllingSwipe(true),
      mEventsHaveStartedNewGesture(false),
      mRegisteredWithRefreshDriver(false) {
  SendSwipeEvent(eSwipeGestureStart, 0, 0.0, aSwipeStartEvent.mTimeStamp);
  ProcessEvent(aSwipeStartEvent, /* aProcessingFirstEvent = */ true);
}

void SwipeTracker::Destroy() { UnregisterFromRefreshDriver(); }

SwipeTracker::~SwipeTracker() {
  MOZ_ASSERT(!mRegisteredWithRefreshDriver,
             "Destroy needs to be called before deallocating");
}

double SwipeTracker::SwipeSuccessTargetValue() const {
  return (mSwipeDirection == dom::SimpleGestureEvent_Binding::DIRECTION_RIGHT)
             ? -1.0
             : 1.0;
}

double SwipeTracker::ClampToAllowedRange(double aGestureAmount) const {
  // gestureAmount needs to stay between -1 and 0 when swiping right and
  // between 0 and 1 when swiping left.
  double min =
      (mSwipeDirection == dom::SimpleGestureEvent_Binding::DIRECTION_RIGHT)
          ? -1.0
          : 0.0;
  double max =
      (mSwipeDirection == dom::SimpleGestureEvent_Binding::DIRECTION_LEFT)
          ? 1.0
          : 0.0;
  return clamped(aGestureAmount, min, max);
}

bool SwipeTracker::ComputeSwipeSuccess() const {
  double targetValue = SwipeSuccessTargetValue();

  // If the fingers were moving away from the target direction when they were
  // lifted from the touchpad, abort the swipe.
  if (mCurrentVelocity * targetValue <
      -StaticPrefs::widget_swipe_velocity_twitch_tolerance()) {
    return false;
  }

  return (mGestureAmount * targetValue +
          mCurrentVelocity * targetValue *
              StaticPrefs::widget_swipe_success_velocity_contribution()) >=
         kSwipeSuccessThreshold;
}

nsEventStatus SwipeTracker::ProcessEvent(
    const PanGestureInput& aEvent, bool aProcessingFirstEvent /* = false */) {
  // If the fingers have already been lifted or the swipe direction is where
  // navigation is impossible, don't process this event for swiping.
  if (!mEventsAreControllingSwipe || !SwipingInAllowedDirection()) {
    // Return nsEventStatus_eConsumeNoDefault for events from the swipe gesture
    // and nsEventStatus_eIgnore for events of subsequent scroll gestures.
    if (aEvent.mType == PanGestureInput::PANGESTURE_MAYSTART ||
        aEvent.mType == PanGestureInput::PANGESTURE_START) {
      mEventsHaveStartedNewGesture = true;
    }
    return mEventsHaveStartedNewGesture ? nsEventStatus_eIgnore
                                        : nsEventStatus_eConsumeNoDefault;
  }

  double delta = -aEvent.mPanDisplacement.x /
                 mWidget.GetDefaultScaleInternal() /
                 StaticPrefs::widget_swipe_whole_page_pixel_size();
  mGestureAmount = ClampToAllowedRange(mGestureAmount + delta);
  if (aEvent.mType != PanGestureInput::PANGESTURE_END) {
    if (!aProcessingFirstEvent) {
      double elapsedSeconds = std::max(
          0.008, (aEvent.mTimeStamp - mLastEventTimeStamp).ToSeconds());
      mCurrentVelocity = delta / elapsedSeconds;
    }
    mLastEventTimeStamp = aEvent.mTimeStamp;
  }

  const bool computedSwipeSuccess = ComputeSwipeSuccess();

  // The velocity component might push us over the success threshold, in which
  // case we want to pass the success threshold in the event we send so that the
  // UI draws as 100% opacity to indicate such. We don't want to include the
  // velocity in the amount we put on the event if we aren't over the success
  // threshold because that would lead to the opacity decreasing even if the
  // user continues to increase the swipe distance. If we do compute swipe
  // success here and the user does not lift their fingers and then decreases
  // the total swipe so that we go below the success threshold the opacity would
  // also decrease in that case but that seems okay.
  // We don't want above tweak if we move the UI along with the opacity change
  // since it forces the UI element jump to the last position and jump back to
  // the original position if the navigation didn't happen.
  double eventAmount = mGestureAmount;
  if (computedSwipeSuccess &&
      StaticPrefs::browser_swipe_navigation_icon_move_distance() == 0) {
    eventAmount = kSwipeSuccessThreshold;
    if (mGestureAmount < 0.f) {
      eventAmount = -eventAmount;
    }
  }

  // If ComputeSwipeSuccess returned false because the users fingers were moving
  // slightly away from the target direction then we do not want to display
  // the UI as if we were at the success threshold as that would give a false
  // indication that navigation would happen.
  if (!computedSwipeSuccess && (eventAmount >= kSwipeSuccessThreshold ||
                                eventAmount <= -kSwipeSuccessThreshold)) {
    eventAmount = 0.999 * kSwipeSuccessThreshold;
    if (mGestureAmount < 0.f) {
      eventAmount = -eventAmount;
    }
  }

  SendSwipeEvent(eSwipeGestureUpdate, 0, eventAmount, aEvent.mTimeStamp);

  if (aEvent.mType == PanGestureInput::PANGESTURE_END) {
    mEventsAreControllingSwipe = false;
    if (computedSwipeSuccess) {
      // Let's use same timestamp as previous event because this is caused by
      // the preceding event.
      SendSwipeEvent(eSwipeGesture, mSwipeDirection, 0.0, aEvent.mTimeStamp);
      UnregisterFromRefreshDriver();
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("SwipeTracker::SwipeFinished",
                                 [swipeTracker = RefPtr<SwipeTracker>(this),
                                  timeStamp = aEvent.mTimeStamp] {
                                   swipeTracker->SwipeFinished(timeStamp);
                                 }));
    } else {
      StartAnimating(eventAmount, 0.0);
    }
  }

  return nsEventStatus_eConsumeNoDefault;
}

void SwipeTracker::StartAnimating(double aStartValue, double aTargetValue) {
  mAxis.SetPosition(aStartValue);
  mAxis.SetDestination(aTargetValue);
  mAxis.SetVelocity(mCurrentVelocity);

  mLastAnimationFrameTime = TimeStamp::Now();

  // Add ourselves as a refresh driver observer. The refresh driver
  // will call WillRefresh for each animation frame until we
  // unregister ourselves.
  MOZ_ASSERT(!mRegisteredWithRefreshDriver);
  if (mRefreshDriver) {
    mRefreshDriver->AddRefreshObserver(this, FlushType::Style,
                                       "Swipe animation");
    mRegisteredWithRefreshDriver = true;
  }
}

void SwipeTracker::WillRefresh(mozilla::TimeStamp aTime) {
  TimeStamp now = TimeStamp::Now();
  mAxis.Simulate(now - mLastAnimationFrameTime);
  mLastAnimationFrameTime = now;

  bool isFinished =
      mAxis.IsFinished(1.0 / StaticPrefs::widget_swipe_whole_page_pixel_size());
  mGestureAmount = (isFinished ? mAxis.GetDestination() : mAxis.GetPosition());
  SendSwipeEvent(eSwipeGestureUpdate, 0, mGestureAmount, now);

  if (isFinished) {
    UnregisterFromRefreshDriver();
    SwipeFinished(now);
  }
}

void SwipeTracker::CancelSwipe(const TimeStamp& aTimeStamp) {
  SendSwipeEvent(eSwipeGestureEnd, 0, 0.0, aTimeStamp);
}

void SwipeTracker::SwipeFinished(const TimeStamp& aTimeStamp) {
  SendSwipeEvent(eSwipeGestureEnd, 0, 0.0, aTimeStamp);
  mWidget.SwipeFinished();
}

void SwipeTracker::UnregisterFromRefreshDriver() {
  if (mRegisteredWithRefreshDriver) {
    MOZ_ASSERT(mRefreshDriver, "How were we able to register, then?");
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Style);
  }
  mRegisteredWithRefreshDriver = false;
}

/* static */ WidgetSimpleGestureEvent SwipeTracker::CreateSwipeGestureEvent(
    EventMessage aMsg, nsIWidget* aWidget,
    const LayoutDeviceIntPoint& aPosition, const TimeStamp& aTimeStamp) {
  // XXX Why isn't this initialized with nsCocoaUtils::InitInputEvent()?
  WidgetSimpleGestureEvent geckoEvent(true, aMsg, aWidget);
  geckoEvent.mModifiers = 0;
  // XXX How about geckoEvent.mTime?
  geckoEvent.mTimeStamp = aTimeStamp;
  geckoEvent.mRefPoint = aPosition;
  geckoEvent.mButtons = 0;
  return geckoEvent;
}

bool SwipeTracker::SendSwipeEvent(EventMessage aMsg, uint32_t aDirection,
                                  double aDelta, const TimeStamp& aTimeStamp) {
  WidgetSimpleGestureEvent geckoEvent =
      CreateSwipeGestureEvent(aMsg, &mWidget, mEventPosition, aTimeStamp);
  geckoEvent.mDirection = aDirection;
  geckoEvent.mDelta = aDelta;
  geckoEvent.mAllowedDirections = mAllowedDirections;
  return mWidget.DispatchWindowEvent(geckoEvent);
}

// static
bool SwipeTracker::CanTriggerSwipe(const PanGestureInput& aPanInput) {
  if (StaticPrefs::widget_disable_swipe_tracker()) {
    return false;
  }

  if (aPanInput.mType != PanGestureInput::PANGESTURE_START) {
    return false;
  }

  // Only initiate horizontal tracking for events whose horizontal element is
  // at least eight times larger than its vertical element. This minimizes
  // performance problems with vertical scrolls (by minimizing the possibility
  // that they'll be misinterpreted as horizontal swipes), while still
  // tolerating a small vertical element to a true horizontal swipe.  The number
  // '8' was arrived at by trial and error.
  return std::abs(aPanInput.mPanDisplacement.x) >
         std::abs(aPanInput.mPanDisplacement.y) * 8;
}

}  // namespace mozilla
