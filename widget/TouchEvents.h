/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TouchEvents_h__
#define mozilla_TouchEvents_h__

#include <stdint.h>

#include "mozilla/dom/Touch.h"
#include "mozilla/MouseEvents.h"
#include "nsAutoPtr.h"
#include "nsIDOMSimpleGestureEvent.h"
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

class WidgetGestureNotifyEvent : public WidgetGUIEvent
{
public:
  virtual WidgetGestureNotifyEvent* AsGestureNotifyEvent() override
  {
    return this;
  }

  WidgetGestureNotifyEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget *aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eGestureNotifyEventClass)
    , panDirection(ePanNone)
    , displayPanFeedback(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
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

  enum ePanDirection
  {
    ePanNone,
    ePanVertical,
    ePanHorizontal,
    ePanBoth
  };

  ePanDirection panDirection;
  bool displayPanFeedback;

  void AssignGestureNotifyEventData(const WidgetGestureNotifyEvent& aEvent,
                                    bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    panDirection = aEvent.panDirection;
    displayPanFeedback = aEvent.displayPanFeedback;
  }
};

/******************************************************************************
 * mozilla::WidgetTouchEvent
 ******************************************************************************/

class WidgetSimpleGestureEvent : public WidgetMouseEventBase
{
public:
  virtual WidgetSimpleGestureEvent* AsSimpleGestureEvent() override
  {
    return this;
  }

  WidgetSimpleGestureEvent(bool aIsTrusted, EventMessage aMessage,
                           nsIWidget* aWidget)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                           eSimpleGestureEventClass)
    , allowedDirections(0)
    , direction(0)
    , delta(0.0)
    , clickCount(0)
  {
  }

  WidgetSimpleGestureEvent(const WidgetSimpleGestureEvent& aOther)
    : WidgetMouseEventBase(aOther.mFlags.mIsTrusted, aOther.mMessage,
                           aOther.widget, eSimpleGestureEventClass)
    , allowedDirections(aOther.allowedDirections)
    , direction(aOther.direction)
    , delta(aOther.delta)
    , clickCount(0)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eSimpleGestureEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetSimpleGestureEvent* result =
      new WidgetSimpleGestureEvent(false, mMessage, nullptr);
    result->AssignSimpleGestureEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // See nsIDOMSimpleGestureEvent for values
  uint32_t allowedDirections;
  // See nsIDOMSimpleGestureEvent for values
  uint32_t direction;
  // Delta for magnify and rotate events
  double delta;
  // The number of taps for tap events
  uint32_t clickCount;

  // XXX Not tested by test_assign_event_data.html
  void AssignSimpleGestureEventData(const WidgetSimpleGestureEvent& aEvent,
                                    bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    // allowedDirections isn't copied
    direction = aEvent.direction;
    delta = aEvent.delta;
    clickCount = aEvent.clickCount;
  }
};

/******************************************************************************
 * mozilla::WidgetTouchEvent
 ******************************************************************************/

class WidgetTouchEvent : public WidgetInputEvent
{
public:
  typedef nsTArray<nsRefPtr<mozilla::dom::Touch>> TouchArray;
  typedef nsAutoTArray<nsRefPtr<mozilla::dom::Touch>, 10> AutoTouchArray;

  virtual WidgetTouchEvent* AsTouchEvent() override { return this; }

  WidgetTouchEvent()
  {
  }

  WidgetTouchEvent(const WidgetTouchEvent& aOther)
    : WidgetInputEvent(aOther.mFlags.mIsTrusted, aOther.mMessage, aOther.widget,
                       eTouchEventClass)
  {
    modifiers = aOther.modifiers;
    time = aOther.time;
    timeStamp = aOther.timeStamp;
    touches.AppendElements(aOther.touches);
    mFlags.mCancelable = mMessage != eTouchCancel;
    MOZ_COUNT_CTOR(WidgetTouchEvent);
  }

  WidgetTouchEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
    : WidgetInputEvent(aIsTrusted, aMessage, aWidget, eTouchEventClass)
  {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mFlags.mCancelable = mMessage != eTouchCancel;
  }

  virtual ~WidgetTouchEvent()
  {
    MOZ_COUNT_DTOR(WidgetTouchEvent);
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eTouchEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetTouchEvent* result = new WidgetTouchEvent(false, mMessage, nullptr);
    result->AssignTouchEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  TouchArray touches;

  void AssignTouchEventData(const WidgetTouchEvent& aEvent, bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    // Assign*EventData() assume that they're called only new instance.
    MOZ_ASSERT(touches.IsEmpty());
    touches.AppendElements(aEvent.touches);
  }
};

} // namespace mozilla

#endif // mozilla_TouchEvents_h__
