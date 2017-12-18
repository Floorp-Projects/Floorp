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
#include "nsString.h"

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

  enum OrientType
  {
    eVertical,
    eHorizontal,
    eBoth
  };

  InternalScrollPortEvent(bool aIsTrusted, EventMessage aMessage,
                          nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eScrollPortEventClass)
    , mOrient(eVertical)
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

  OrientType mOrient;

  void AssignScrollPortEventData(const InternalScrollPortEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    mOrient = aEvent.mOrient;
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
 * mOriginator is a weak pointer (does not hold a strong reference).
 ******************************************************************************/

class InternalFormEvent : public WidgetEvent
{
public:
  virtual InternalFormEvent* AsFormEvent() override { return this; }

  InternalFormEvent(bool aIsTrusted, EventMessage aMessage)
    : WidgetEvent(aIsTrusted, aMessage, eFormEventClass)
    , mOriginator(nullptr)
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

  nsIContent* mOriginator;

  void AssignFormEventData(const InternalFormEvent& aEvent, bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // Don't copy mOriginator due to a weak pointer.
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

  nsCOMPtr<dom::DataTransfer> mClipboardData;

  void AssignClipboardEventData(const InternalClipboardEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    mClipboardData = aEvent.mClipboardData;
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
    , mFromRaise(false)
    , mIsRefocus(false)
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

  bool mFromRaise;
  bool mIsRefocus;

  void AssignFocusEventData(const InternalFocusEvent& aEvent, bool aCopyTargets)
  {
    AssignUIEventData(aEvent, aCopyTargets);

    mFromRaise = aEvent.mFromRaise;
    mIsRefocus = aEvent.mIsRefocus;
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
    , mElapsedTime(0.0)
  {
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

  nsString mPropertyName;
  nsString mPseudoElement;
  float mElapsedTime;

  void AssignTransitionEventData(const InternalTransitionEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    mPropertyName = aEvent.mPropertyName;
    mElapsedTime = aEvent.mElapsedTime;
    mPseudoElement = aEvent.mPseudoElement;
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
    , mElapsedTime(0.0)
  {
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

  nsString mAnimationName;
  nsString mPseudoElement;
  float mElapsedTime;

  void AssignAnimationEventData(const InternalAnimationEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    mAnimationName = aEvent.mAnimationName;
    mElapsedTime = aEvent.mElapsedTime;
    mPseudoElement = aEvent.mPseudoElement;
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
