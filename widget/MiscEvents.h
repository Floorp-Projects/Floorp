/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MiscEvents_h__
#define mozilla_MiscEvents_h__

#include <stdint.h>

#include "mozilla/BasicEvents.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsITransferable.h"

namespace mozilla {

/******************************************************************************
 * mozilla::WidgetContentCommandEvent
 ******************************************************************************/

class WidgetContentCommandEvent : public WidgetGUIEvent
{
public:
  WidgetContentCommandEvent(bool aIsTrusted, uint32_t aMessage,
                            nsIWidget* aWidget,
                            bool aOnlyEnabledCheck = false) :
    WidgetGUIEvent(aIsTrusted, aMessage, aWidget, NS_CONTENT_COMMAND_EVENT),
    mOnlyEnabledCheck(aOnlyEnabledCheck), mSucceeded(false), mIsEnabled(false)
  {
  }

  // NS_CONTENT_COMMAND_PASTE_TRANSFERABLE
  nsCOMPtr<nsITransferable> mTransferable; // [in]

  // NS_CONTENT_COMMAND_SCROLL
  // for mScroll.mUnit
  enum
  {
    eCmdScrollUnit_Line,
    eCmdScrollUnit_Page,
    eCmdScrollUnit_Whole
  };

  struct ScrollInfo
  {
    ScrollInfo() :
      mAmount(0), mUnit(eCmdScrollUnit_Line), mIsHorizontal(false)
    {
    }

    int32_t mAmount;    // [in]
    uint8_t mUnit;      // [in]
    bool mIsHorizontal; // [in]
  } mScroll;

  bool mOnlyEnabledCheck; // [in]

  bool mSucceeded; // [out]
  bool mIsEnabled; // [out]
};

/******************************************************************************
 * mozilla::WidgetCommandEvent
 *
 * This sends a command to chrome.  If you want to request what is performed
 * in focused content, you should use WidgetContentCommandEvent instead.
 *
 * XXX Should be |WidgetChromeCommandEvent|?
 ******************************************************************************/

class WidgetCommandEvent : public WidgetGUIEvent
{
public:
  WidgetCommandEvent(bool aIsTrusted, nsIAtom* aEventType,
                     nsIAtom* aCommand, nsIWidget* aWidget) :
    WidgetGUIEvent(aIsTrusted, NS_USER_DEFINED_EVENT, aWidget,
                   NS_COMMAND_EVENT),
    command(aCommand)
  {
    userType = aEventType;
  }

  nsCOMPtr<nsIAtom> command;

  // XXX Not tested by test_assign_event_data.html
  void AssignCommandEventData(const WidgetCommandEvent& aEvent,
                              bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    // command must have been initialized with the constructor.
  }
};

/******************************************************************************
 * mozilla::WidgetPluginEvent
 *
 * This event delivers only a native event to focused plugin.
 ******************************************************************************/

class WidgetPluginEvent : public WidgetGUIEvent
{
public:
  WidgetPluginEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetGUIEvent(aIsTrusted, aMessage, aWidget, NS_PLUGIN_EVENT),
    retargetToFocusedDocument(false)
  {
  }

  // If true, this event needs to be retargeted to focused document.
  // Otherwise, never retargeted. Defaults to false.
  bool retargetToFocusedDocument;
};

} // namespace mozilla

#endif // mozilla_MiscEvents_h__
