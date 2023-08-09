/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MiscEvents_h__
#define mozilla_MiscEvents_h__

#include <stdint.h>

#include "mozilla/BasicEvents.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsITransferable.h"
#include "nsString.h"

namespace mozilla {

namespace dom {
class PBrowserParent;
class PBrowserChild;
}  // namespace dom

/******************************************************************************
 * mozilla::WidgetContentCommandEvent
 ******************************************************************************/

class WidgetContentCommandEvent : public WidgetGUIEvent {
 public:
  virtual WidgetContentCommandEvent* AsContentCommandEvent() override {
    return this;
  }

  WidgetContentCommandEvent(bool aIsTrusted, EventMessage aMessage,
                            nsIWidget* aWidget, bool aOnlyEnabledCheck = false)
      : WidgetGUIEvent(aIsTrusted, aMessage, aWidget,
                       eContentCommandEventClass),
        mOnlyEnabledCheck(aOnlyEnabledCheck),
        mSucceeded(false),
        mIsEnabled(false) {}

  virtual WidgetEvent* Duplicate() const override {
    // This event isn't an internal event of any DOM event.
    NS_ASSERTION(!IsAllowedToDispatchDOMEvent(),
                 "WidgetQueryContentEvent needs to support Duplicate()");
    MOZ_CRASH("WidgetQueryContentEvent doesn't support Duplicate()");
    return nullptr;
  }

  // eContentCommandInsertText
  mozilla::Maybe<nsString> mString;  // [in]

  // eContentCommandPasteTransferable
  nsCOMPtr<nsITransferable> mTransferable;  // [in]

  // eContentCommandScroll
  // for mScroll.mUnit
  enum { eCmdScrollUnit_Line, eCmdScrollUnit_Page, eCmdScrollUnit_Whole };

  struct ScrollInfo {
    ScrollInfo()
        : mAmount(0), mUnit(eCmdScrollUnit_Line), mIsHorizontal(false) {}

    int32_t mAmount;     // [in]
    uint8_t mUnit;       // [in]
    bool mIsHorizontal;  // [in]
  } mScroll;

  bool mOnlyEnabledCheck;  // [in]

  bool mSucceeded;  // [out]
  bool mIsEnabled;  // [out]

  void AssignContentCommandEventData(const WidgetContentCommandEvent& aEvent,
                                     bool aCopyTargets) {
    AssignGUIEventData(aEvent, aCopyTargets);

    mString = aEvent.mString;
    mScroll = aEvent.mScroll;
    mOnlyEnabledCheck = aEvent.mOnlyEnabledCheck;
    mSucceeded = aEvent.mSucceeded;
    mIsEnabled = aEvent.mIsEnabled;
  }
};

/******************************************************************************
 * mozilla::WidgetCommandEvent
 *
 * This sends a command to chrome.  If you want to request what is performed
 * in focused content, you should use WidgetContentCommandEvent instead.
 *
 * XXX Should be |WidgetChromeCommandEvent|?
 ******************************************************************************/

class WidgetCommandEvent : public WidgetGUIEvent {
 public:
  virtual WidgetCommandEvent* AsCommandEvent() override { return this; }

 protected:
  WidgetCommandEvent(bool aIsTrusted, nsAtom* aEventType, nsAtom* aCommand,
                     nsIWidget* aWidget, const WidgetEventTime* aTime = nullptr)
      : WidgetGUIEvent(aIsTrusted, eUnidentifiedEvent, aWidget,
                       eCommandEventClass, aTime),
        mCommand(aCommand) {
    mSpecifiedEventType = aEventType;
  }

 public:
  /**
   * Constructor to initialize an app command.  This is the only case to
   * initialize this class as a command in C++ stack.
   */
  WidgetCommandEvent(bool aIsTrusted, nsAtom* aCommand, nsIWidget* aWidget,
                     const WidgetEventTime* aTime = nullptr)
      : WidgetCommandEvent(aIsTrusted, nsGkAtoms::onAppCommand, aCommand,
                           aWidget, aTime) {}

  /**
   * Constructor to initialize as internal event of dom::CommandEvent.
   */
  WidgetCommandEvent()
      : WidgetCommandEvent(false, nullptr, nullptr, nullptr, nullptr) {}

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eCommandEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetCommandEvent* result = new WidgetCommandEvent(
        false, mSpecifiedEventType, mCommand, nullptr, this);
    result->AssignCommandEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  RefPtr<nsAtom> mCommand;

  // XXX Not tested by test_assign_event_data.html
  void AssignCommandEventData(const WidgetCommandEvent& aEvent,
                              bool aCopyTargets) {
    AssignGUIEventData(aEvent, aCopyTargets);

    // mCommand must have been initialized with the constructor.
  }
};

}  // namespace mozilla

#endif  // mozilla_MiscEvents_h__
