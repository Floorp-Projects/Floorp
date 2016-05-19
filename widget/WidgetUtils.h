/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WidgetUtils_h
#define mozilla_WidgetUtils_h

#include "mozilla/EventForwards.h"
#include "mozilla/gfx/Matrix.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsRect.h"

class nsPIDOMWindowOuter;

namespace mozilla {

// NB: these must match up with pseudo-enum in nsIScreen.idl.
enum ScreenRotation {
  ROTATION_0 = 0,
  ROTATION_90,
  ROTATION_180,
  ROTATION_270,

  ROTATION_COUNT
};

gfx::Matrix ComputeTransformForRotation(const nsIntRect& aBounds,
                                        ScreenRotation aRotation);

gfx::Matrix ComputeTransformForUnRotation(const nsIntRect& aBounds,
                                          ScreenRotation aRotation);

nsIntRect RotateRect(nsIntRect aRect,
                     const nsIntRect& aBounds,
                     ScreenRotation aRotation);

namespace widget {

class WidgetUtils
{
public:
  /**
   * Shutdown() is called when "xpcom-will-shutdown" is notified.  This is
   * useful when you need to observe the notification in XP level code under
   * widget.
   */
  static void Shutdown();

  /**
   * Starting at the docshell item for the passed in DOM window this looks up
   * the docshell tree until it finds a docshell item that has a widget.
   */
  static already_AddRefed<nsIWidget> DOMWindowToWidget(nsPIDOMWindowOuter* aDOMWindow);

  /**
   * Compute our keyCode value (NS_VK_*) from an ASCII character.
   */
  static uint32_t ComputeKeyCodeFromChar(uint32_t aCharCode);

  /**
   * Get unshifted charCode and shifted charCode for aKeyCode if the keyboad
   * layout is a Latin keyboard layout.
   *
   * @param aKeyCode            Our keyCode (NS_VK_*).
   * @param aIsCapsLock         TRUE if CapsLock is Locked.  Otherwise, FALSE.
   *                            This is used only when aKeyCode is NS_VK_[0-9].
   * @param aUnshiftedCharCode  CharCode for aKeyCode without Shift key.
   *                            This may be zero if aKeyCode key doesn't input
   *                            a Latin character.
   *                            Note that must not be nullptr.
   * @param aShiftedCharCode    CharCode for aKeyCOde with Shift key.
   *                            This is always 0 when aKeyCode isn't
   *                            NS_VK_[A-Z].
   *                            Note that must not be nullptr.
   */
  static void GetLatinCharCodeForKeyCode(uint32_t aKeyCode,
                                         bool aIsCapsLock,
                                         uint32_t* aUnshiftedCharCode,
                                         uint32_t* aShiftedCharCode);

  /**
  * Does device have touch support
  */
  static uint32_t IsTouchDeviceSupportPresent();

  /**
   * Send bidi keyboard information to content process
   */
  static void SendBidiKeyboardInfoToContent();
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_WidgetUtils_h
