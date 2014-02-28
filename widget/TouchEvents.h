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
  virtual WidgetGestureNotifyEvent* AsGestureNotifyEvent() MOZ_OVERRIDE
  {
    return this;
  }

  WidgetGestureNotifyEvent(bool aIsTrusted, uint32_t aMessage,
                           nsIWidget *aWidget) :
    WidgetGUIEvent(aIsTrusted, aMessage, aWidget, NS_GESTURENOTIFY_EVENT),
    panDirection(ePanNone), displayPanFeedback(false)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    // This event isn't an internal event of any DOM event.
    MOZ_CRASH("WidgetGestureNotifyEvent doesn't support Duplicate()");
    return nullptr;
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
};

/******************************************************************************
 * mozilla::WidgetTouchEvent
 ******************************************************************************/

class WidgetSimpleGestureEvent : public WidgetMouseEventBase
{
public:
  virtual WidgetSimpleGestureEvent* AsSimpleGestureEvent() MOZ_OVERRIDE
  {
    return this;
  }

  WidgetSimpleGestureEvent(bool aIsTrusted, uint32_t aMessage,
                           nsIWidget* aWidget)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                           NS_SIMPLE_GESTURE_EVENT)
    , allowedDirections(0)
    , direction(0)
    , delta(0.0)
    , clickCount(0)
  {
  }

  WidgetSimpleGestureEvent(const WidgetSimpleGestureEvent& aOther)
    : WidgetMouseEventBase(aOther.mFlags.mIsTrusted, aOther.message,
                           aOther.widget, NS_SIMPLE_GESTURE_EVENT)
    , allowedDirections(aOther.allowedDirections)
    , direction(aOther.direction)
    , delta(aOther.delta)
    , clickCount(0)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_SIMPLE_GESTURE_EVENT,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetSimpleGestureEvent* result =
      new WidgetSimpleGestureEvent(false, message, nullptr);
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
  virtual WidgetTouchEvent* AsTouchEvent() MOZ_OVERRIDE { return this; }

  WidgetTouchEvent()
  {
  }

  WidgetTouchEvent(const WidgetTouchEvent& aOther) :
    WidgetInputEvent(aOther.mFlags.mIsTrusted, aOther.message, aOther.widget,
                     NS_TOUCH_EVENT)
  {
    modifiers = aOther.modifiers;
    time = aOther.time;
    touches.AppendElements(aOther.touches);
    mFlags.mCancelable = message != NS_TOUCH_CANCEL;
    MOZ_COUNT_CTOR(WidgetTouchEvent);
  }

  WidgetTouchEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetInputEvent(aIsTrusted, aMessage, aWidget, NS_TOUCH_EVENT)
  {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
    mFlags.mCancelable = message != NS_TOUCH_CANCEL;
  }

  virtual ~WidgetTouchEvent()
  {
    MOZ_COUNT_DTOR(WidgetTouchEvent);
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_TOUCH_EVENT,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetTouchEvent* result = new WidgetTouchEvent(false, message, nullptr);
    result->AssignTouchEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsTArray<nsRefPtr<mozilla::dom::Touch>> touches;

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
