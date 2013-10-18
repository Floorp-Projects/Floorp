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

  enum ePanDirection
  {
    ePanNone,
    ePanVertical,
    ePanHorizontal,
    ePanBoth
  };

  ePanDirection panDirection;
  bool displayPanFeedback;

  // XXX Not tested by test_assign_event_data.html
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
  virtual WidgetSimpleGestureEvent* AsSimpleGestureEvent() MOZ_OVERRIDE
  {
    return this;
  }

  WidgetSimpleGestureEvent(bool aIsTrusted, uint32_t aMessage,
                           nsIWidget* aWidget, uint32_t aDirection,
                           double aDelta) :
    WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                         NS_SIMPLE_GESTURE_EVENT),
    allowedDirections(0), direction(aDirection), delta(aDelta), clickCount(0)
  {
  }

  WidgetSimpleGestureEvent(const WidgetSimpleGestureEvent& aOther) :
    WidgetMouseEventBase(aOther.mFlags.mIsTrusted,
                         aOther.message, aOther.widget,
                         NS_SIMPLE_GESTURE_EVENT),
    allowedDirections(aOther.allowedDirections), direction(aOther.direction),
    delta(aOther.delta), clickCount(0)
  {
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

  WidgetTouchEvent(bool aIsTrusted, WidgetTouchEvent* aEvent) :
    WidgetInputEvent(aIsTrusted, aEvent->message, aEvent->widget,
                     NS_TOUCH_EVENT)
  {
    modifiers = aEvent->modifiers;
    time = aEvent->time;
    touches.AppendElements(aEvent->touches);
    MOZ_COUNT_CTOR(WidgetTouchEvent);
  }

  WidgetTouchEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetInputEvent(aIsTrusted, aMessage, aWidget, NS_TOUCH_EVENT)
  {
    MOZ_COUNT_CTOR(WidgetTouchEvent);
  }

  virtual ~WidgetTouchEvent()
  {
    MOZ_COUNT_DTOR(WidgetTouchEvent);
  }

  nsTArray<nsRefPtr<mozilla::dom::Touch>> touches;

  void AssignTouchEventData(const WidgetTouchEvent& aEvent, bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    // Currently, we don't need to copy touches.
  }
};

} // namespace mozilla

#endif // mozilla_TouchEvents_h__
