/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinIMEHandler_h_
#define WinIMEHandler_h_

#include "nscore.h"
#include <windows.h>

class nsWindow;

namespace mozilla {
namespace widget {

/**
 * IMEHandler class is a mediator class.  On Windows, there are two IME API
 * sets: One is IMM which is legacy API set. The other is TSF which is modern
 * API set. By using this class, non-IME handler classes don't need to worry
 * that we're in which mode.
 */
class IMEHandler MOZ_FINAL
{
public:
  static void Initialize();
  static void Terminate();

  /**
   * When the message is not needed to handle anymore by the caller, this
   * returns true.  Otherwise, false.
   * Additionally, if aEatMessage is true, the caller shouldn't call next
   * wndproc anymore.
   */
  static bool ProcessMessage(nsWindow* aWindow, UINT aMessage,
                             WPARAM& aWParam, LPARAM& aLParam,
                             LRESULT* aRetValue, bool& aEatMessage);

  /**
   * "Kakutei-Undo" of ATOK or WXG (both of them are Japanese IME) causes
   * strange WM_KEYDOWN/WM_KEYUP/WM_CHAR message pattern.  So, when this
   * returns true, the caller needs to be careful for processing the messages.
   */
  static bool IsDoingKakuteiUndo(HWND aWnd);

#ifdef DEBUG
  /**
   * Returns true when current keyboard layout has IME.  Otherwise, false.
   */
  static bool CurrentKeyboardLayoutHasIME();
#endif // #ifdef DEBUG

private:
#ifdef NS_ENABLE_TSF
  static bool sIsInTSFMode;
#endif // #ifdef NS_ENABLE_TSF
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef WinIMEHandler_h_
