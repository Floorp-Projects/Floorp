/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_WidgetUtils_h__
#define __mozilla_widget_WidgetUtils_h__

#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"

namespace mozilla {
namespace widget {

class WidgetUtils
{
public:

  /**
   * Starting at the docshell item for the passed in DOM window this looks up
   * the docshell tree until it finds a docshell item that has a widget.
   */
  static already_AddRefed<nsIWidget> DOMWindowToWidget(nsIDOMWindow *aDOMWindow);

  /**
   * Compute our keyCode value (NS_VK_*) from an ASCII character.
   */
  static PRUint32 ComputeKeyCodeFromChar(PRUint32 aCharCode);

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
   *                            Note that must not be NULL.
   * @param aShiftedCharCode    CharCode for aKeyCOde with Shift key.
   *                            This is always 0 when aKeyCode isn't
   *                            NS_VK_[A-Z].
   *                            Note that must not be NULL.
   */
  static void GetLatinCharCodeForKeyCode(PRUint32 aKeyCode,
                                         bool aIsCapsLock,
                                         PRUint32* aUnshiftedCharCode,
                                         PRUint32* aShiftedCharCode);
};

} // namespace widget
} // namespace mozilla

#endif
