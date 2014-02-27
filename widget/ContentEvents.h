/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentEvents_h__
#define mozilla_ContentEvents_h__

#include <stdint.h>

#include "mozilla/BasicEvents.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/EventTarget.h"
#include "nsCOMPtr.h"
#include "nsRect.h"
#include "nsStringGlue.h"

class nsIContent;

namespace mozilla {

/******************************************************************************
 * mozilla::InternalScrollPortEvent
 ******************************************************************************/

class InternalScrollPortEvent : public WidgetGUIEvent
{
public:
  virtual InternalScrollPortEvent* AsScrollPortEvent() MOZ_OVERRIDE
  {
    return this;
  }

  enum orientType
  {
    vertical   = 0,
    horizontal = 1,
    both       = 2
  };

  InternalScrollPortEvent(bool aIsTrusted, uint32_t aMessage,
                          nsIWidget* aWidget) :
    WidgetGUIEvent(aIsTrusted, aMessage, aWidget, NS_SCROLLPORT_EVENT),
    orient(vertical)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_SCROLLPORT_EVENT,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalScrollPortEvent* result =
      new InternalScrollPortEvent(false, message, nullptr);
    result->AssignScrollPortEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  orientType orient;

  void AssignScrollPortEventData(const InternalScrollPortEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    orient = aEvent.orient;
  }
};

/******************************************************************************
 * mozilla::InternalScrollPortEvent
 ******************************************************************************/

class InternalScrollAreaEvent : public WidgetGUIEvent
{
public:
  virtual InternalScrollAreaEvent* AsScrollAreaEvent() MOZ_OVERRIDE
  {
    return this;
  }

  InternalScrollAreaEvent(bool aIsTrusted, uint32_t aMessage,
                          nsIWidget* aWidget) :
    WidgetGUIEvent(aIsTrusted, aMessage, aWidget, NS_SCROLLAREA_EVENT)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_SCROLLAREA_EVENT,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalScrollAreaEvent* result =
      new InternalScrollAreaEvent(false, message, nullptr);
    result->AssignScrollAreaEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsRect mArea;

  void AssignScrollAreaEventData(const InternalScrollAreaEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    mArea = aEvent.mArea;
  }
};

/******************************************************************************
 * mozilla::InternalFormEvent
 *
 * We hold the originating form control for form submit and reset events.
 * originator is a weak pointer (does not hold a strong reference).
 ******************************************************************************/

class InternalFormEvent : public WidgetEvent
{
public:
  virtual InternalFormEvent* AsFormEvent() MOZ_OVERRIDE { return this; }

  InternalFormEvent(bool aIsTrusted, uint32_t aMessage) :
    WidgetEvent(aIsTrusted, aMessage, NS_FORM_EVENT),
    originator(nullptr)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_FORM_EVENT,
               "Duplicate() must be overridden by sub class");
    InternalFormEvent* result = new InternalFormEvent(false, message);
    result->AssignFormEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsIContent *originator;

  void AssignFormEventData(const InternalFormEvent& aEvent, bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // Don't copy originator due to a weak pointer.
  }
};

/******************************************************************************
 * mozilla::InternalClipboardEvent
 ******************************************************************************/

class InternalClipboardEvent : public WidgetEvent
{
public:
  virtual InternalClipboardEvent* AsClipboardEvent() MOZ_OVERRIDE
  {
    return this;
  }

  InternalClipboardEvent(bool aIsTrusted, uint32_t aMessage) :
    WidgetEvent(aIsTrusted, aMessage, NS_CLIPBOARD_EVENT)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_CLIPBOARD_EVENT,
               "Duplicate() must be overridden by sub class");
    InternalClipboardEvent* result = new InternalClipboardEvent(false, message);
    result->AssignClipboardEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsCOMPtr<dom::DataTransfer> clipboardData;

  void AssignClipboardEventData(const InternalClipboardEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    clipboardData = aEvent.clipboardData;
  }
};

/******************************************************************************
 * mozilla::InternalFocusEvent
 ******************************************************************************/

class InternalFocusEvent : public InternalUIEvent
{
public:
  virtual InternalFocusEvent* AsFocusEvent() MOZ_OVERRIDE { return this; }

  InternalFocusEvent(bool aIsTrusted, uint32_t aMessage) :
    InternalUIEvent(aIsTrusted, aMessage, NS_FOCUS_EVENT),
    fromRaise(false), isRefocus(false)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_FOCUS_EVENT,
               "Duplicate() must be overridden by sub class");
    InternalFocusEvent* result = new InternalFocusEvent(false, message);
    result->AssignFocusEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  /// The possible related target
  nsCOMPtr<dom::EventTarget> relatedTarget;

  bool fromRaise;
  bool isRefocus;

  void AssignFocusEventData(const InternalFocusEvent& aEvent, bool aCopyTargets)
  {
    AssignUIEventData(aEvent, aCopyTargets);

    relatedTarget = aCopyTargets ? aEvent.relatedTarget : nullptr;
    fromRaise = aEvent.fromRaise;
    isRefocus = aEvent.isRefocus;
  }
};

/******************************************************************************
 * mozilla::InternalTransitionEvent
 ******************************************************************************/

class InternalTransitionEvent : public WidgetEvent
{
public:
  virtual InternalTransitionEvent* AsTransitionEvent() MOZ_OVERRIDE
  {
    return this;
  }

  InternalTransitionEvent(bool aIsTrusted, uint32_t aMessage)
    : WidgetEvent(aIsTrusted, aMessage, NS_TRANSITION_EVENT)
    , elapsedTime(0.0)
  {
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_TRANSITION_EVENT,
               "Duplicate() must be overridden by sub class");
    InternalTransitionEvent* result =
      new InternalTransitionEvent(false, message);
    result->AssignTransitionEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsString propertyName;
  float elapsedTime;
  nsString pseudoElement;

  void AssignTransitionEventData(const InternalTransitionEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    propertyName = aEvent.propertyName;
    elapsedTime = aEvent.elapsedTime;
    pseudoElement = aEvent.pseudoElement;
  }
};

/******************************************************************************
 * mozilla::InternalAnimationEvent
 ******************************************************************************/

class InternalAnimationEvent : public WidgetEvent
{
public:
  virtual InternalAnimationEvent* AsAnimationEvent() MOZ_OVERRIDE
  {
    return this;
  }

  InternalAnimationEvent(bool aIsTrusted, uint32_t aMessage)
    : WidgetEvent(aIsTrusted, aMessage, NS_ANIMATION_EVENT)
    , elapsedTime(0.0)
  {
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(eventStructType == NS_ANIMATION_EVENT,
               "Duplicate() must be overridden by sub class");
    InternalAnimationEvent* result = new InternalAnimationEvent(false, message);
    result->AssignAnimationEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  nsString animationName;
  float elapsedTime;
  nsString pseudoElement;

  void AssignAnimationEventData(const InternalAnimationEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    animationName = aEvent.animationName;
    elapsedTime = aEvent.elapsedTime;
    pseudoElement = aEvent.pseudoElement;
  }
};

} // namespace mozilla

#endif // mozilla_ContentEvents_h__
