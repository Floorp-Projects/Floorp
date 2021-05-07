/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TouchEvents_h__
#define mozilla_TouchEvents_h__

#include <stdint.h>

#include "mozilla/dom/Touch.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

/******************************************************************************
 * mozilla::WidgetGestureNotifyEvent
 *
 * This event is the first event generated when the user touches
 * the screen with a finger, and it's meant to decide what kind
 * of action we'll use for that touch interaction.
 *
 * The event is dispatched to the layout and based on what is underneath
 * the initial contact point it's then decided if we should pan
 * (finger scrolling) or drag the target element.
 ******************************************************************************/

class WidgetGestureNotifyEvent : public WidgetGUIEvent {
 public:
  virtual WidgetGestureNotifyEvent* AsGestureNotifyEvent() override {
    return this;
  }

  WidgetGestureNotifyEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eGestureNotifyEventClass),
        mPanDirection(ePanNone),
        mDisplayPanFeedback(false) {}

  virtual WidgetEvent* Duplicate() const override {
    // XXX Looks like this event is handled only in PostHandleEvent() of
    //     EventStateManager.  Therefore, it might be possible to handle this
    //     in PreHandleEvent() and not to dispatch as a DOM event into the DOM
    //     tree like ContentQueryEvent.  Then, this event doesn't need to
    //     support Duplicate().
    MOZ_ASSERT(mClass == eGestureNotifyEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetGestureNotifyEvent* result =
        new WidgetGestureNotifyEvent(false, mMessage, nullptr);
    result->AssignGestureNotifyEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  typedef int8_t PanDirectionType;
  enum PanDirection : PanDirectionType {
    ePanNone,
    ePanVertical,
    ePanHorizontal,
    ePanBoth
  };

  PanDirection mPanDirection;
  bool mDisplayPanFeedback;

  void AssignGestureNotifyEventData(const WidgetGestureNotifyEvent& aEvent,
                                    bool aCopyTargets) {
    AssignGUIEventData(aEvent, aCopyTargets);

    mPanDirection = aEvent.mPanDirection;
    mDisplayPanFeedback = aEvent.mDisplayPanFeedback;
  }
};

/******************************************************************************
 * mozilla::WidgetSimpleGestureEvent
 ******************************************************************************/

class WidgetSimpleGestureEvent : public WidgetMouseEventBase {
 public:
  virtual WidgetSimpleGestureEvent* AsSimpleGestureEvent() override {
    return this;
  }

  WidgetSimpleGestureEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                             eSimpleGestureEventClass),
        mAllowedDirections(0),
        mDirection(0),
        mClickCount(0),
        mDelta(0.0) {}

  WidgetSimpleGestureEvent(const WidgetSimpleGestureEvent& aOther)
      : WidgetMouseEventBase(aOther.IsTrusted(), aOther.mMessage,
                             aOther.mWidget, eSimpleGestureEventClass),
        mAllowedDirections(aOther.mAllowedDirections),
        mDirection(aOther.mDirection),
        mClickCount(0),
        mDelta(aOther.mDelta) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eSimpleGestureEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetSimpleGestureEvent* result =
        new WidgetSimpleGestureEvent(false, mMessage, nullptr);
    result->AssignSimpleGestureEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // See SimpleGestureEvent.webidl for values
  uint32_t mAllowedDirections;
  // See SimpleGestureEvent.webidl for values
  uint32_t mDirection;
  // The number of taps for tap events
  uint32_t mClickCount;
  // Delta for magnify and rotate events
  double mDelta;

  // XXX Not tested by test_assign_event_data.html
  void AssignSimpleGestureEventData(const WidgetSimpleGestureEvent& aEvent,
                                    bool aCopyTargets) {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    // mAllowedDirections isn't copied
    mDirection = aEvent.mDirection;
    mDelta = aEvent.mDelta;
    mClickCount = aEvent.mClickCount;
  }
};

/******************************************************************************
 * mozilla::WidgetTouchEvent
 ******************************************************************************/

class WidgetTouchEvent : public WidgetInputEvent {
 public:
  typedef nsTArray<RefPtr<mozilla::dom::Touch>> TouchArray;
  typedef AutoTArray<RefPtr<mozilla::dom::Touch>, 10> AutoTouchArray;
  typedef AutoTouchArray::base_type TouchArrayBase;

  virtual WidgetTouchEvent* AsTouchEvent() override { return this; }

  MOZ_COUNTED_DEFAULT_CTOR(WidgetTouchEvent)

  WidgetTouchEvent(const WidgetTouchEvent& aOther)
      : WidgetInputEvent(aOther.IsTrusted(), aOther.mMessage, aOther.mWidget,
                         eTouchEventClass),
        mInputSource(aOther.mInputSource) {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mModifiers = aOther.mModifiers;
    mTime = aOther.mTime;
    mTimeStamp = aOther.mTimeStamp;
    mTouches.AppendElements(aOther.mTouches);
    mFlags.mCancelable = mMessage != eTouchCancel;
    mFlags.mHandledByAPZ = aOther.mFlags.mHandledByAPZ;
  }

  WidgetTouchEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
      : WidgetInputEvent(aIsTrusted, aMessage, aWidget, eTouchEventClass) {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mFlags.mCancelable = mMessage != eTouchCancel;
  }

  MOZ_COUNTED_DTOR_OVERRIDE(WidgetTouchEvent)

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eTouchEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetTouchEvent* result = new WidgetTouchEvent(false, mMessage, nullptr);
    result->AssignTouchEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  TouchArray mTouches;
  uint16_t mInputSource = 5;  // MouseEvent_Binding::MOZ_SOURCE_TOUCH

  void AssignTouchEventData(const WidgetTouchEvent& aEvent, bool aCopyTargets) {
    AssignInputEventData(aEvent, aCopyTargets);

    // Assign*EventData() assume that they're called only new instance.
    MOZ_ASSERT(mTouches.IsEmpty());
    mTouches.AppendElements(aEvent.mTouches);
    mInputSource = aEvent.mInputSource;
  }
};

}  // namespace mozilla

#endif  // mozilla_TouchEvents_h__
