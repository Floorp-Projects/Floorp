/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinNativeEventData_h_
#define mozilla_widget_WinNativeEventData_h_

#include <windows.h>

#include "mozilla/EventForwards.h"
#include "mozilla/widget/WinModifierKeyState.h"

namespace mozilla {
namespace widget {

/**
 * WinNativeKeyEventData is used by nsIWidget::OnWindowedPluginKeyEvent() and
 * related IPC methods.  This class cannot hold any pointers and references
 * since which are not available in different process.
 */

class WinNativeKeyEventData final
{
public:
  UINT mMessage;
  WPARAM mWParam;
  LPARAM mLParam;
  Modifiers mModifiers;

private:
  uintptr_t mKeyboardLayout;

public:
  WinNativeKeyEventData(UINT aMessage,
                        WPARAM aWParam,
                        LPARAM aLParam,
                        const ModifierKeyState& aModifierKeyState)
    : mMessage(aMessage)
    , mWParam(aWParam)
    , mLParam(aLParam)
    , mModifiers(aModifierKeyState.GetModifiers())
    , mKeyboardLayout(reinterpret_cast<uintptr_t>(::GetKeyboardLayout(0)))
  {
  }

  HKL GetKeyboardLayout() const
  {
    return reinterpret_cast<HKL>(mKeyboardLayout);
  }
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_widget_WinNativeEventData_h_
