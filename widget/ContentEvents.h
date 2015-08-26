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
  virtual InternalScrollPortEvent* AsScrollPortEvent() override
  {
    return this;
  }

  enum orientType
  {
    vertical   = 0,
    horizontal = 1,
    both       = 2
  };

  InternalScrollPortEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eScrollPortEventClass)
    , orient(vertical)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eScrollPortEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalScrollPortEvent* result =
      new InternalScrollPortEvent(false, mMessage, nullptr);
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
  virtual InternalScrollAreaEvent* AsScrollAreaEvent() override
  {
    return this;
  }

  InternalScrollAreaEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eScrollAreaEventClass)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eScrollAreaEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalScrollAreaEvent* result =
      new InternalScrollAreaEvent(false, mMessage, nullptr);
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
  virtual InternalFormEvent* AsFormEvent() override { return this; }

  InternalFormEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetEvent(aIsTrusted, aMessage, eFormEventClass)
    , originator(nullptr)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eFormEventClass,
               "Duplicate() must be overridden by sub class");
    InternalFormEvent* result = new InternalFormEvent(false, mMessage);
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
  virtual InternalClipboardEvent* AsClipboardEvent() override
  {
    return this;
  }

  InternalClipboardEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetEvent(aIsTrusted, aMessage, eClipboardEventClass)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eClipboardEventClass,
               "Duplicate() must be overridden by sub class");
    InternalClipboardEvent* result =
      new InternalClipboardEvent(false, mMessage);
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
  virtual InternalFocusEvent* AsFocusEvent() override { return this; }

  InternalFocusEvent(bool aIsTrusted, EventMessage aMessage)
    : InternalUIEvent(aIsTrusted, aMessage, eFocusEventClass)
    , fromRaise(false)
    , isRefocus(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eFocusEventClass,
               "Duplicate() must be overridden by sub class");
    InternalFocusEvent* result = new InternalFocusEvent(false, mMessage);
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
  virtual InternalTransitionEvent* AsTransitionEvent() override
  {
    return this;
  }

  InternalTransitionEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetEvent(aIsTrusted, aMessage, eTransitionEventClass)
    , elapsedTime(0.0)
  {
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eTransitionEventClass,
               "Duplicate() must be overridden by sub class");
    InternalTransitionEvent* result =
      new InternalTransitionEvent(false, mMessage);
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
  virtual InternalAnimationEvent* AsAnimationEvent() override
  {
    return this;
  }

  InternalAnimationEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetEvent(aIsTrusted, aMessage, eAnimationEventClass)
    , elapsedTime(0.0)
  {
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eAnimationEventClass,
               "Duplicate() must be overridden by sub class");
    InternalAnimationEvent* result =
      new InternalAnimationEvent(false, mMessage);
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

/******************************************************************************
 * mozilla::InternalSVGZoomEvent
 ******************************************************************************/

class InternalSVGZoomEvent : public WidgetGUIEvent
{
public:
  virtual InternalSVGZoomEvent* AsSVGZoomEvent() override { return this; }

  InternalSVGZoomEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetGUIEvent(aIsTrusted, aMessage, nullptr, eSVGZoomEventClass)
  {
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eSVGZoomEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    InternalSVGZoomEvent* result = new InternalSVGZoomEvent(false, mMessage);
    result->AssignSVGZoomEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  void AssignSVGZoomEventData(const InternalSVGZoomEvent& aEvent,
                              bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);
  }
};

/******************************************************************************
 * mozilla::InternalSMILTimeEvent
 ******************************************************************************/

class InternalSMILTimeEvent : public InternalUIEvent
{
public:
  virtual InternalSMILTimeEvent* AsSMILTimeEvent() override
  {
    return this;
  }

  InternalSMILTimeEvent(bool aIsTrusted, EventMessage aMessage)
    : InternalUIEvent(aIsTrusted, aMessage, eSMILTimeEventClass)
  {
    mFlags.mBubbles = false;
    mFlags.mCancelable = false;
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eSMILTimeEventClass,
               "Duplicate() must be overridden by sub class");
    InternalSMILTimeEvent* result = new InternalSMILTimeEvent(false, mMessage);
    result->AssignSMILTimeEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  void AssignSMILTimeEventData(const InternalSMILTimeEvent& aEvent,
                               bool aCopyTargets)
  {
    AssignUIEventData(aEvent, aCopyTargets);
  }
};


} // namespace mozilla

#endif // mozilla_ContentEvents_h__
